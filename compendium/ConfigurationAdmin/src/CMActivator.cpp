/*=============================================================================

 Library: CppMicroServices

 Copyright (c) The CppMicroServices developers. See the COPYRIGHT
 file at the top-level directory of this distribution and at
 https://github.com/CppMicroServices/CppMicroServices/COPYRIGHT .

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 =============================================================================*/

#include <functional>
#include <stdexcept>

#include "CMActivator.hpp"
#include "CMConstants.hpp"

#include "boost/asio/async_result.hpp"
#include "boost/asio/packaged_task.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/thread_pool.hpp"

using cppmicroservices::logservice::SeverityLevel;

namespace cppmicroservices {
namespace cmimpl {

#define ASYNC_USE_INLINE

class AsyncWorkServiceImpl final
  : public ::cppmicroservices::async::detail::AsyncWorkService
{
public:
  AsyncWorkServiceImpl()
  {
#if defined(ASYNC_USE_THREADPOOL)
    threadpool = std::make_shared<boost::asio::thread_pool>(1);
#endif
  }

  ~AsyncWorkServiceImpl()
  {
#if defined(ASYNC_USE_THREADPOOL)
    try {
      if (threadpool) {
        try {
          threadpool->join();
        } catch (...) {
          //
        }
      }
    } catch (...) {
      //
    }
#endif
  }

  std::future<void> post(std::function<void()>&& task) override
  {
#if defined(ASYNC_USE_INLINE)
    std::packaged_task<void()> pt([](){});
    std::future<void> f = pt.get_future();
    task();
    pt();
    return f;
#elif defined(ASYNC_USE_ASYNC)
    std::future<void> f = std::async(std::launch::async, task);
    return f;
#elif defined(ASYNC_USE_THREADPOOL)
    std::packaged_task<void()> pt(
      [task = std::move(task)]() mutable { task(); });

    using Sig = void();
    using Result = boost::asio::async_result<decltype(pt), Sig>;
    using Handler = typename Result::completion_handler_type;

    Handler handler(std::forward<decltype(pt)>(pt));
    Result result(handler);

    boost::asio::post(threadpool->get_executor(),
                      [handler = std::move(handler)]() mutable { handler(); });

    return result.get();
#endif
  }

  void post(std::packaged_task<void()>&& task) override
  {
#if defined(ASYNC_USE_INLINE)
    task();
#elif defined(ASYNC_USE_ASYNC)
    std::future<void> f = std::async(
      std::launch::async, [task = std::move(task)]() mutable { task(); });
#elif defined(ASYNC_USE_THREADPOOL)
    using Sig = void();
    using Result = boost::asio::async_result<decltype(task), Sig>;
    using Handler = typename Result::completion_handler_type;

    Handler handler(std::forward<decltype(task)>(task));
    Result result(handler);

    boost::asio::post(threadpool->get_executor(),
                      [handler = std::move(handler)]() mutable { handler(); });
#endif
  }

#if defined(ASYNC_USE_THREADPOOL)
private:
  std::shared_ptr<boost::asio::thread_pool> threadpool;
#endif
};

void CMActivator::Start(BundleContext context)
{
  runtimeContext = context;
  // Create the Logger object used by this runtime
  logger = std::make_shared<CMLogger>(context);
  logger->Log(SeverityLevel::LOG_DEBUG, "Starting CM bundle");

  asyncWorkService = std::make_shared<AsyncWorkServiceImpl>();

  // Create ConfigurationAdminImpl
  configAdminImpl = std::make_shared<ConfigurationAdminImpl>(
    runtimeContext, logger, asyncWorkService);
  // Add bundle listener
  bundleListenerToken = context.AddBundleListener(
    std::bind(&CMActivator::BundleChanged, this, std::placeholders::_1));
  // HACK: Workaround for lack of Bundle Tracker. Iterate over all bundles and call the tracker method manually
  for (const auto& bundle : context.GetBundles()) {
    if (cppmicroservices::Bundle::State::STATE_ACTIVE == bundle.GetState()) {
      cppmicroservices::BundleEvent evt(
        cppmicroservices::BundleEvent::BUNDLE_STARTED, bundle);
      BundleChanged(evt);
    }
  }
  // Publish ConfigurationAdmin service
  configAdminReg =
    context.RegisterService<cppmicroservices::service::cm::ConfigurationAdmin>(
      configAdminImpl);
}

void CMActivator::Stop(cppmicroservices::BundleContext context)
{
  try {
    // remove the bundle listener
    context.RemoveListener(std::move(bundleListenerToken));
    // remove the runtime service from the framework
    configAdminReg.Unregister();
    // clear bundle registry
    {
      std::lock_guard<std::mutex> l(bundleRegMutex);
      bundleRegistry.clear();
    }
    // Clean up the ConfigurationAdminImpl
    configAdminImpl = nullptr;
    logger->Log(SeverityLevel::LOG_DEBUG, "CM Bundle stopped.");
  } catch (...) {
    logger->Log(SeverityLevel::LOG_DEBUG,
                "Exception while stopping the CM bundle",
                std::current_exception());
  }
  logger = nullptr;
  runtimeContext = nullptr;
}

void CMActivator::CreateExtension(const cppmicroservices::Bundle& bundle)
{
  auto const& headers = bundle.GetHeaders();
  // bundle has no "cm" configuration
  if (headers.find(CMConstants::CM_KEY) == std::end(headers)) {
    logger->Log(SeverityLevel::LOG_DEBUG,
                "No CM Configuration found in bundle " +
                  bundle.GetSymbolicName());
    return;
  }

  auto extensionFound = false;
  {
    std::lock_guard<std::mutex> l(bundleRegMutex);
    extensionFound =
      (bundleRegistry.find(bundle.GetBundleId()) != std::end(bundleRegistry));
  }
  // This bundle's configuration has not been loaded, so create the extension which will load it
  if (extensionFound) {
    logger->Log(SeverityLevel::LOG_DEBUG,
                "CM Configuration already loaded from bundle " +
                  bundle.GetSymbolicName());
    return;
  }

  logger->Log(SeverityLevel::LOG_DEBUG,
              "Creating CMBundleExtension ... " + bundle.GetSymbolicName());
  try {
    auto const& cmMetadata =
      cppmicroservices::ref_any_cast<cppmicroservices::AnyMap>(
        headers.at(CMConstants::CM_KEY));
    auto be = std::make_unique<CMBundleExtension>(
      bundle.GetBundleContext(), cmMetadata, configAdminImpl, logger);
    {
      std::lock_guard<std::mutex> l(bundleRegMutex);
      bundleRegistry.emplace(bundle.GetBundleId(), std::move(be));
    }
  } catch (const std::exception&) {
    logger->Log(SeverityLevel::LOG_WARNING,
                "Failed to create CMBundleExtension for " +
                  bundle.GetSymbolicName(),
                std::current_exception());
  }
}

void CMActivator::RemoveExtension(const cppmicroservices::Bundle& bundle)
{
  auto const& headers = bundle.GetHeaders();
  // bundle has no "cm" configuration
  if (headers.find(CMConstants::CM_KEY) == std::end(headers)) {
    logger->Log(SeverityLevel::LOG_DEBUG,
                "No CM Configuration found in bundle " +
                  bundle.GetSymbolicName());
    return;
  }

  bool extensionFound = false;
  {
    std::lock_guard<std::mutex> l(bundleRegMutex);
    auto extensionIt = bundleRegistry.find(bundle.GetBundleId());
    if (extensionIt != std::end(bundleRegistry)) {
      bundleRegistry.erase(extensionIt);
      extensionFound = true;
    }
  }
  if (extensionFound) {
    logger->Log(SeverityLevel::LOG_DEBUG,
                "Removed CMBundleExtension for " + bundle.GetSymbolicName());
    return;
  }
  logger->Log(SeverityLevel::LOG_DEBUG,
              "Found no CMBundleExtension for " + bundle.GetSymbolicName());
}

void CMActivator::BundleChanged(const cppmicroservices::BundleEvent& evt)
{
  auto bundle = evt.GetBundle();
  const auto eventType = evt.GetType();
  if (bundle ==
      runtimeContext.GetBundle()) // skip events for this (runtime) bundle
  {
    return;
  }

  // TODO: revisit to include LAZY_ACTIVATION when supported by the framework
  if (cppmicroservices::BundleEvent::BUNDLE_STARTED == eventType) {
    CreateExtension(bundle);
  } else if (cppmicroservices::BundleEvent::BUNDLE_STOPPING == eventType) {
    RemoveExtension(bundle);
  }
  // else ignore
}
} // cmimpl
} // cppmicroservices

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(
  cppmicroservices::cmimpl::CMActivator) // NOLINT
