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

#include "SCRAsyncWorkService.hpp"

#include "boost/asio/async_result.hpp"
#include "boost/asio/packaged_task.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/thread_pool.hpp"

namespace cppmicroservices {
namespace scrimpl {

/**
 * FallbackAsyncWorkService represents the fallback strategy in the event
 * that a AsyncWorkService is not present within the framework. It implements
 * the public interface for AsyncWorkService and is created in the event that
 * a user-provided service was not given or if the user-provided service
 * which implements the AsyncWorkService interface was unregistered.
 */
class FallbackAsyncWorkService final
  : public cppmicroservices::async::detail::AsyncWorkService
{
public:
  FallbackAsyncWorkService(
    const std::shared_ptr<cppmicroservices::logservice::LogService>& logger_)
    : logger(logger_)
  {
    Initialize();
  }

  void Initialize()
  {
    threadpool = std::make_shared<boost::asio::thread_pool>(2);
  }

  void Shutdown()
  {
    if (threadpool) {
      try {
        threadpool->join();
        threadpool->stop();
        threadpool.reset();
      } catch (...) {
        auto exceptionPtr = std::current_exception();
        std::string msg =
          "An exception has occured while trying to shutdown "
          "the fallback cppmicroservices::async::detail::AsyncWorkService "
          "instance.";
        logger->Log(cppmicroservices::logservice::SeverityLevel::LOG_WARNING,
                    msg,
                    exceptionPtr);
      }
    }
  }

  ~FallbackAsyncWorkService() { Shutdown(); }

  void post(std::packaged_task<void()>&& task) override
  {
    if (threadpool) {
      using Sig = void();
      using Result = boost::asio::async_result<decltype(task), Sig>;
      using Handler = typename Result::completion_handler_type;

      Handler handler(std::forward<decltype(task)>(task));
      Result result(handler);

      boost::asio::post(
        threadpool->get_executor(),
        [handler = std::move(handler)]() mutable { handler(); });
    }
  }

private:
  std::shared_ptr<boost::asio::thread_pool> threadpool;
  std::shared_ptr<cppmicroservices::logservice::LogService> logger;
};

SCRAsyncWorkService::SCRAsyncWorkService(
  cppmicroservices::BundleContext context,
  const std::shared_ptr<SCRLogger>& logger_)
  : scrContext(context)
  , serviceTracker(
      std::make_unique<cppmicroservices::ServiceTracker<
        cppmicroservices::async::detail::AsyncWorkService>>(context, this))
  , asyncWorkService(std::make_shared<FallbackAsyncWorkService>(logger_))
  , logger(logger_)
{
  serviceTracker->Open();
}

SCRAsyncWorkService::~SCRAsyncWorkService() noexcept
{
  asyncWorkService.reset();
}

void SCRAsyncWorkService::StopTracking()
{
  if (serviceTracker) {
    serviceTracker->Close();
    serviceTracker.reset();
  }
}

std::shared_ptr<cppmicroservices::async::detail::AsyncWorkService>
SCRAsyncWorkService::AddingService(
  const ServiceReference<cppmicroservices::async::detail::AsyncWorkService>&
    reference)
{
  auto currAsync = std::atomic_load(&asyncWorkService);
  std::shared_ptr<cppmicroservices::async::detail::AsyncWorkService> newService;
  if (reference) {
    try {
      newService =
        scrContext
          .GetService<cppmicroservices::async::detail::AsyncWorkService>(
            reference);
      if (newService) {
        std::atomic_store(&asyncWorkService, newService);
      }
    } catch (...) {
      auto exceptionPtr = std::current_exception();
      std::string msg =
        "An exception was caught while retrieving an instance of "
        "cppmicroservices::async::detail::AsyncWorkService. Falling "
        "back to the default.";
      logger->Log(cppmicroservices::logservice::SeverityLevel::LOG_WARNING,
                  msg,
                  exceptionPtr);
    }
  }
  return newService;
}

void SCRAsyncWorkService::ModifiedService(
  const ServiceReference<
    cppmicroservices::async::detail::AsyncWorkService>& /* reference */,
  const std::shared_ptr<
    cppmicroservices::async::detail::AsyncWorkService>& /* service */)
{
  // no-op
}

void SCRAsyncWorkService::RemovedService(
  const ServiceReference<
    cppmicroservices::async::detail::AsyncWorkService>& /* reference */,
  const std::shared_ptr<cppmicroservices::async::detail::AsyncWorkService>&
    service)
{
  auto currAsync = std::atomic_load(&asyncWorkService);
  if (service == currAsync) {
    // replace existing asyncWorkService with a nullptr asyncWorkService
    std::shared_ptr<cppmicroservices::async::detail::AsyncWorkService>
      newService = std::make_shared<FallbackAsyncWorkService>(logger);
    std::atomic_store(&asyncWorkService, newService);
  }
}

void SCRAsyncWorkService::post(std::packaged_task<void()>&& task)
{
  auto currAsync = std::atomic_load(&asyncWorkService);
  currAsync->post(std::move(task));
}

}
}
