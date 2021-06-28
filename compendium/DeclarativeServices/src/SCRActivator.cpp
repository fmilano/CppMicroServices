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

#include "SCRActivator.hpp"
#include "SCRLogger.hpp"
#include "ServiceComponentRuntimeImpl.hpp"
#include "manager/ComponentManager.hpp"
#include "manager/ReferenceManager.hpp"
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include "cppmicroservices/SharedLibraryException.h"
#include "cppmicroservices/servicecomponent/ComponentConstants.hpp"
#include "cppmicroservices/servicecomponent/runtime/dto/ComponentConfigurationDTO.hpp"
#include "cppmicroservices/servicecomponent/runtime/dto/ComponentDescriptionDTO.hpp"
#include "cppmicroservices/servicecomponent/runtime/dto/ReferenceDTO.hpp"

#include "cppmicroservices/detail/ScopeGuard.h"

#include "boost/asio/async_result.hpp"
#include "boost/asio/packaged_task.hpp"
#include "boost/asio/post.hpp"

using cppmicroservices::logservice::SeverityLevel;
using cppmicroservices::service::component::ComponentConstants::
  SERVICE_COMPONENT;

namespace cppmicroservices {
namespace scrimpl {

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

void SCRActivator::Start(BundleContext context)
{
  runtimeContext = context;

  // limit the number of threads to 2. There is currently no use case to warrant increasing it.
  //threadpool = std::make_shared<boost::asio::thread_pool>(2);
  threadpool = nullptr;

  asyncWorkService = std::make_shared<AsyncWorkServiceImpl>();

  // Create the component registry
  componentRegistry = std::make_shared<ComponentRegistry>();

  // Create the Logger object used by this runtime
  logger = std::make_shared<SCRLogger>(context);
  logger->Log(SeverityLevel::LOG_DEBUG, "Starting SCR bundle");
  // Add bundle listener
  bundleListenerToken = context.AddBundleListener(
    std::bind(&SCRActivator::BundleChanged, this, std::placeholders::_1));
  // HACK: Workaround for lack of Bundle Tracker. Iterate over all bundles and call the tracker method manually
  for (const auto& bundle : context.GetBundles()) {
    if (bundle.GetState() == cppmicroservices::Bundle::State::STATE_ACTIVE) {
      cppmicroservices::BundleEvent evt(
        cppmicroservices::BundleEvent::BUNDLE_STARTED, bundle);
      BundleChanged(evt);
    }
  }
  // Publish ServiceComponentRuntimeService
  auto service = std::make_shared<ServiceComponentRuntimeImpl>(
    runtimeContext, componentRegistry, logger);
  scrServiceReg =
    context.RegisterService<ServiceComponentRuntime>(std::move(service));
}

void SCRActivator::Stop(cppmicroservices::BundleContext context)
{
  try {
    cppmicroservices::detail::ScopeGuard joinThreadPool{ [this]() {
      if (threadpool) {
        try {
          threadpool->join();
        } catch (...) {
          logger->Log(SeverityLevel::LOG_WARNING,
                      "Exception while joining the threadpool",
                      std::current_exception());
        }
      }
    } };
    // remove the bundle listener
    context.RemoveListener(std::move(bundleListenerToken));
    // remove the runtime service from the framework
    scrServiceReg.Unregister();
    // dispose all components created by SCR
    const auto bundles = context.GetBundles();
    for (auto const& bundle : bundles) {
      DisposeExtension(bundle);
    }

    // clear bundle registry
    {
      std::lock_guard<std::mutex> l(bundleRegMutex);
      bundleRegistry.clear();
    }
    // clear component registry
    componentRegistry->Clear();

    logger->Log(SeverityLevel::LOG_DEBUG, "SCR Bundle stopped.");
  } catch (...) {
    logger->Log(
      SeverityLevel::LOG_DEBUG,
      "Exception while stopping the declarative services runtime bundle",
      std::current_exception());
  }
  logger->StopTracking();
}

void SCRActivator::CreateExtension(const cppmicroservices::Bundle& bundle)
{
  const auto& headers = bundle.GetHeaders();
  // bundle has no "scr" property
  if (headers.count(SERVICE_COMPONENT) == 0u) {
    logger->Log(SeverityLevel::LOG_DEBUG,
                "No SCR components found in bundle " +
                  bundle.GetSymbolicName());
    return;
  }

  bool extensionFound = false;
  {
    std::lock_guard<std::mutex> l(bundleRegMutex);
    extensionFound = (bundleRegistry.count(bundle.GetBundleId()) != 0u);
  }
  // bundle components have not been loaded, so create the extension which will load the components
  if (!extensionFound) {
    logger->Log(SeverityLevel::LOG_DEBUG,
                "Creating SCRBundleExtension ... " + bundle.GetSymbolicName());
    try {
      auto const& scrMap =
        ref_any_cast<cppmicroservices::AnyMap>(headers.at(SERVICE_COMPONENT));
      auto ba = std::make_unique<SCRBundleExtension>(bundle.GetBundleContext(),
                                                     scrMap,
                                                     componentRegistry,
                                                     logger,
                                                     threadpool,
                                                     asyncWorkService);
      {
        std::lock_guard<std::mutex> l(bundleRegMutex);
        bundleRegistry.insert(
          std::make_pair(bundle.GetBundleId(), std::move(ba)));
      }
    } catch (const cppmicroservices::SharedLibraryException&) {
      throw;
    } catch (const std::exception&) {
      logger->Log(SeverityLevel::LOG_DEBUG,
                  "Failed to create SCRBundleExtension for " +
                    bundle.GetSymbolicName(),
                  std::current_exception());
    }
  } else {
    logger->Log(SeverityLevel::LOG_DEBUG,
                "SCR components already loaded from bundle " +
                  bundle.GetSymbolicName());
  }
}

void SCRActivator::DisposeExtension(const cppmicroservices::Bundle& bundle)
{
  const auto& headers = bundle.GetHeaders();
  // bundle has no scr-component property
  if (headers.count(SERVICE_COMPONENT) == 0u) {
    logger->Log(SeverityLevel::LOG_DEBUG,
                "Found No SCR Metadata for " + bundle.GetSymbolicName());
    return;
  }

  bool extensionFound = false;
  {
    std::lock_guard<std::mutex> l(bundleRegMutex);
    extensionFound = (bundleRegistry.count(bundle.GetBundleId()) != 0u);
  }
  if (extensionFound) {
    logger->Log(SeverityLevel::LOG_DEBUG,
                "Found SCRBundleExtension for " + bundle.GetSymbolicName());
    // remove the bundle extension object from the map.
    {
      std::lock_guard<std::mutex> l(bundleRegMutex);
      bundleRegistry.erase(bundle.GetBundleId());
    }
  } else {
    logger->Log(SeverityLevel::LOG_DEBUG,
                "Found No SCRBundleExtension for " + bundle.GetSymbolicName());
  }
}

void SCRActivator::BundleChanged(const cppmicroservices::BundleEvent& evt)
{
  auto bundle = evt.GetBundle();
  const auto eventType = evt.GetType();
  if (bundle ==
      runtimeContext.GetBundle()) // skip events for this (runtime) bundle
  {
    return;
  }

  // TODO: revisit to include LAZY_ACTIVATION when supported by the framework
  if (eventType == cppmicroservices::BundleEvent::BUNDLE_STARTED) {
    CreateExtension(bundle);
  } else if (eventType == cppmicroservices::BundleEvent::BUNDLE_STOPPING) {
    DisposeExtension(bundle);
  }
  // else ignore
}
} // scrimpl
} // cppmicroservices

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(
  cppmicroservices::scrimpl::SCRActivator) // NOLINT
