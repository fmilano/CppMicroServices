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

#ifndef MOCKS_HPP
#define MOCKS_HPP

#include "gmock/gmock.h"

#include "cppmicroservices/cm/ManagedService.hpp"
#include "cppmicroservices/cm/ManagedServiceFactory.hpp"
#include "cppmicroservices/logservice/LogService.hpp"

#include "../src/ConfigurationAdminPrivate.hpp"

#include "cppmicroservices/asyncworkservice/AsyncWorkService.hpp"

#include "boost/asio/async_result.hpp"
#include "boost/asio/packaged_task.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/thread_pool.hpp"

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

/**
 * This class is used in tests where the logger is required and the test
 * needs to verify what is sent to the logger
 */
class MockLogger : public cppmicroservices::logservice::LogService
{
public:
  MOCK_METHOD2(Log,
               void(cppmicroservices::logservice::SeverityLevel,
                    const std::string&));
  MOCK_METHOD3(Log,
               void(cppmicroservices::logservice::SeverityLevel,
                    const std::string&,
                    const std::exception_ptr));
  MOCK_METHOD3(Log,
               void(const cppmicroservices::ServiceReferenceBase&,
                    cppmicroservices::logservice::SeverityLevel,
                    const std::string&));
  MOCK_METHOD4(Log,
               void(const cppmicroservices::ServiceReferenceBase&,
                    cppmicroservices::logservice::SeverityLevel,
                    const std::string&,
                    const std::exception_ptr));
};

/**
 * This class is used in tests where the logger is required but the test
 * does not care if anything is actually sent to the logger
 */
class FakeLogger : public cppmicroservices::logservice::LogService
{
public:
  void Log(cppmicroservices::logservice::SeverityLevel,
           const std::string&) override
  {}
  void Log(cppmicroservices::logservice::SeverityLevel,
           const std::string&,
           const std::exception_ptr) override
  {}
  void Log(const ServiceReferenceBase&,
           cppmicroservices::logservice::SeverityLevel,
           const std::string&) override
  {}
  void Log(const ServiceReferenceBase&,
           cppmicroservices::logservice::SeverityLevel,
           const std::string&,
           const std::exception_ptr) override
  {}
};

/**
 * This class is used in tests which need to verify that the Updated method of
 * a ManagedService is called as expected. The mock can be registered with the
 * Framework to "inject" it into the class under test.
 */
class MockManagedService : public cppmicroservices::service::cm::ManagedService
{
public:
  MOCK_METHOD1(Updated, void(const AnyMap&));
};

/**
 * This class is used in tests which need to verify that the Updated and Removed
 * methods of a ManagedService are called as expected. The mock can be registered
 * with the Framework to "inject" it into the class under test.
 */
class MockManagedServiceFactory
  : public cppmicroservices::service::cm::ManagedServiceFactory
{
public:
  MOCK_METHOD2(Updated, void(const std::string&, const AnyMap&));
  MOCK_METHOD1(Removed, void(const std::string&));
};

/**
 * This class is used in tests of classes which would normally callback to a
 * ConfigurationAdminPrivate. It allows the test to verify that the correct
 * methods are invoked as expected.
 */
class MockConfigurationAdminPrivate : public ConfigurationAdminPrivate
{
public:
  MOCK_METHOD1(AddConfigurations,
               std::vector<ConfigurationAddedInfo>(
                 std::vector<metadata::ConfigurationMetadata>));
  MOCK_METHOD1(RemoveConfigurations, void(std::vector<ConfigurationAddedInfo>));
  MOCK_METHOD1(NotifyConfigurationUpdated, void(const std::string&));
  MOCK_METHOD2(NotifyConfigurationRemoved,
               void(const std::string&, std::uintptr_t));
};
}
}

#endif /* MOCKS_HPP */
