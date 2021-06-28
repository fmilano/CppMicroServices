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

#ifndef __MOCKS_HPP__
#define __MOCKS_HPP__
#include "../src/ComponentRegistry.hpp"
#include "../src/manager/ComponentConfiguration.hpp"
#include "../src/manager/ComponentConfigurationImpl.hpp"
#include "../src/manager/ComponentManager.hpp"
#include "../src/manager/ComponentManagerImpl.hpp"
#include "../src/manager/ReferenceManager.hpp"
#include "../src/manager/ReferenceManagerImpl.hpp"
#include "../src/manager/states/ComponentConfigurationState.hpp"
#include "../src/manager/states/ComponentManagerState.hpp"
#include "cppmicroservices/asyncworkservice/AsyncWorkService.hpp"
#include "gmock/gmock.h"
#include <cppmicroservices/ServiceFactory.h>

#include "boost/asio/async_result.hpp"
#include "boost/asio/packaged_task.hpp"
#include "boost/asio/post.hpp"

namespace cppmicroservices {
namespace scrimpl {
namespace dummy {

struct ServiceImpl
{};
struct Reference1
{};
struct Reference2
{};
struct Reference3
{};

} // namespace dummy

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4373)
#endif

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

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

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

class MockComponentManager : public ComponentManager
{
public:
  MOCK_CONST_METHOD0(GetName, std::string(void));
  MOCK_CONST_METHOD0(GetBundle, cppmicroservices::Bundle(void));
  MOCK_CONST_METHOD0(GetBundleId, unsigned long(void));
  MOCK_METHOD0(Initialize, void(void));
  MOCK_CONST_METHOD0(IsEnabled, bool(void));
  MOCK_METHOD0(Enable, std::shared_future<void>(void));
  MOCK_METHOD0(Disable, std::shared_future<void>(void));
  MOCK_CONST_METHOD0(GetComponentConfigurations,
                     std::vector<std::shared_ptr<ComponentConfiguration>>());
  MOCK_CONST_METHOD0(GetMetadata,
                     std::shared_ptr<const metadata::ComponentMetadata>());
};

class MockComponentRegistry : public ComponentRegistry
{
public:
  MOCK_CONST_METHOD0(GetComponentManagers,
                     std::vector<std::shared_ptr<ComponentManager>>());
  MOCK_CONST_METHOD1(
    GetComponentManagers,
    std::vector<std::shared_ptr<ComponentManager>>(unsigned long));
  MOCK_CONST_METHOD2(GetComponentManager,
                     std::shared_ptr<ComponentManager>(unsigned long,
                                                       const std::string&));
  MOCK_METHOD1(AddComponentManager,
               bool(const std::shared_ptr<ComponentManager>&));
  MOCK_METHOD1(RemoveComponentManager,
               void(const std::shared_ptr<ComponentManager>&));
};

class MockComponentManagerState : public ComponentManagerState
{
public:
  MOCK_METHOD1(Enable, std::shared_future<void>(ComponentManagerImpl&));
  MOCK_METHOD1(Disable, std::shared_future<void>(ComponentManagerImpl&));
  MOCK_CONST_METHOD1(IsEnabled, bool(const ComponentManagerImpl&));
  MOCK_CONST_METHOD1(GetConfigurations,
                     std::vector<std::shared_ptr<ComponentConfiguration>>(
                       const ComponentManagerImpl&));
  MOCK_CONST_METHOD0(GetFuture, std::shared_future<void>());
};

class MockReferenceManager : public ReferenceManager
{
public:
  MOCK_CONST_METHOD0(GetReferenceName, std::string(void));
  MOCK_CONST_METHOD0(GetReferenceScope, std::string(void));
  MOCK_CONST_METHOD0(GetLDAPString, std::string(void));
  MOCK_CONST_METHOD0(IsSatisfied, bool(void));
  MOCK_CONST_METHOD0(IsOptional, bool(void));
  MOCK_CONST_METHOD0(GetBoundReferences,
                     std::set<cppmicroservices::ServiceReferenceBase>());
  MOCK_CONST_METHOD0(GetTargetReferences,
                     std::set<cppmicroservices::ServiceReferenceBase>());
  MOCK_METHOD1(RegisterListener,
               cppmicroservices::ListenerTokenId(
                 std::function<void(const RefChangeNotification&)>));
  MOCK_METHOD1(UnregisterListener, void(cppmicroservices::ListenerTokenId));
  MOCK_METHOD0(StopTracking, void(void));
};

class MockReferenceManagerBaseImpl : public ReferenceManagerBaseImpl
{
public:
  MockReferenceManagerBaseImpl(
    const metadata::ReferenceMetadata& metadata,
    const cppmicroservices::BundleContext& bc,
    std::shared_ptr<cppmicroservices::logservice::LogService> logger,
    const std::string& configName)
    : ReferenceManagerBaseImpl(metadata, bc, logger, configName)
  {}

  MOCK_CONST_METHOD0(GetReferenceName, std::string(void));
  MOCK_CONST_METHOD0(GetReferenceScope, std::string(void));
  MOCK_CONST_METHOD0(GetLDAPString, std::string(void));
  MOCK_CONST_METHOD0(IsSatisfied, bool(void));
  MOCK_CONST_METHOD0(IsOptional, bool(void));
  MOCK_CONST_METHOD0(GetBoundReferences,
                     std::set<cppmicroservices::ServiceReferenceBase>());
  MOCK_CONST_METHOD0(GetTargetReferences,
                     std::set<cppmicroservices::ServiceReferenceBase>());
  MOCK_METHOD1(RegisterListener,
               cppmicroservices::ListenerTokenId(
                 std::function<void(const RefChangeNotification&)>));
  MOCK_METHOD1(UnregisterListener, void(cppmicroservices::ListenerTokenId));
  MOCK_METHOD0(StopTracking, void(void));
};

class MockComponentConfiguration : public ComponentConfiguration
{
public:
  MOCK_CONST_METHOD0(
    GetProperties,
    std::unordered_map<std::string, cppmicroservices::Any>(void));
  MOCK_CONST_METHOD0(GetAllDependencyManagers,
                     std::vector<std::shared_ptr<ReferenceManager>>(void));
  MOCK_CONST_METHOD1(GetDependencyManager,
                     std::shared_ptr<ReferenceManager>(const std::string&));
  MOCK_CONST_METHOD0(GetServiceReference,
                     cppmicroservices::ServiceReferenceBase(void));
  MOCK_CONST_METHOD0(GetRegistry,
                     std::shared_ptr<const ComponentRegistry>(void));
  MOCK_CONST_METHOD0(GetBundle, cppmicroservices::Bundle(void));
  MOCK_CONST_METHOD0(GetId, unsigned long(void));
  MOCK_CONST_METHOD0(GetConfigState, ComponentState(void));
  MOCK_CONST_METHOD0(GetMetadata,
                     std::shared_ptr<const metadata::ComponentMetadata>(void));
};

class MockFactory : public cppmicroservices::ServiceFactory
{
public:
  MOCK_METHOD2(
    GetService,
    InterfaceMapConstPtr(const cppmicroservices::Bundle&,
                         const cppmicroservices::ServiceRegistrationBase&));
  MOCK_METHOD3(UngetService,
               void(const cppmicroservices::Bundle&,
                    const cppmicroservices::ServiceRegistrationBase&,
                    const InterfaceMapConstPtr&));
};

class MockComponentConfigurationState : public ComponentConfigurationState
{
public:
  MOCK_METHOD1(Register, void(ComponentConfigurationImpl&));
  MOCK_METHOD2(
    Activate,
    std::shared_ptr<ComponentInstance>(ComponentConfigurationImpl&,
                                       const cppmicroservices::Bundle&));
  MOCK_METHOD1(Deactivate, void(ComponentConfigurationImpl&));
  MOCK_METHOD4(Rebind,
               void(ComponentConfigurationImpl&,
                    const std::string&,
                    const ServiceReference<void>&,
                    const ServiceReference<void>&));
  MOCK_CONST_METHOD0(GetValue, ComponentState(void));
  MOCK_METHOD0(WaitForTransitionTask, void());
};

class MockComponentInstance
  : public service::component::detail::ComponentInstance
{
public:
  MOCK_METHOD1(
    CreateInstanceAndBindReferences,
    void(const std::shared_ptr<service::component::ComponentContext>&));
  MOCK_METHOD0(UnbindReferences, void(void));
  MOCK_METHOD0(Activate, void(void));
  MOCK_METHOD0(Deactivate, void(void));
  MOCK_METHOD0(Modified, void(void));
  MOCK_METHOD2(InvokeUnbindMethod,
               void(const std::string&,
                    const cppmicroservices::ServiceReferenceBase&));
  MOCK_METHOD2(InvokeBindMethod,
               void(const std::string&,
                    const cppmicroservices::ServiceReferenceBase&));
  MOCK_METHOD0(GetInterfaceMap, cppmicroservices::InterfaceMapPtr(void));
};

class MockComponentContextImpl : public ComponentContextImpl
{
public:
  MockComponentContextImpl(const std::weak_ptr<ComponentConfiguration>& cm)
    : ComponentContextImpl(cm)
  {}
  MOCK_CONST_METHOD0(
    GetProperties,
    std::unordered_map<std::string, cppmicroservices::Any>(void));
  MOCK_CONST_METHOD0(GetBundleContext, cppmicroservices::BundleContext(void));
  MOCK_CONST_METHOD0(GetUsingBundle, cppmicroservices::Bundle(void));
  MOCK_METHOD1(EnableComponent, void(const std::string&));
  MOCK_METHOD1(DisableComponent, void(const std::string&));
  MOCK_CONST_METHOD0(GetServiceReference,
                     cppmicroservices::ServiceReferenceBase(void));
  MOCK_CONST_METHOD2(LocateService,
                     std::shared_ptr<void>(const std::string&,
                                           const std::string&));
  MOCK_CONST_METHOD2(LocateServices,
                     std::vector<std::shared_ptr<void>>(const std::string&,
                                                        const std::string&));
};

class MockComponentManagerImpl : public ComponentManagerImpl
{
public:
  MockComponentManagerImpl(
    std::shared_ptr<const metadata::ComponentMetadata> metadata,
    std::shared_ptr<const ComponentRegistry> registry,
    BundleContext bundleContext,
    std::shared_ptr<cppmicroservices::logservice::LogService> logger,
    std::shared_ptr<boost::asio::thread_pool> pool,
    std::shared_ptr<cppmicroservices::async::detail::AsyncWorkService>
      asyncWorkService)
    : ComponentManagerImpl(metadata,
                           registry,
                           bundleContext,
                           logger,
                           pool,
                           asyncWorkService)
    , statechangecount(0)
  {}
  virtual ~MockComponentManagerImpl() = default;
  void SetState(const std::shared_ptr<ComponentManagerState>& newState)
  {
    auto currentState = GetState();
    ComponentManagerImpl::CompareAndSetState(&currentState, newState);
  }

  MOCK_CONST_METHOD0(GetComponentConfigurations,
                     std::vector<std::shared_ptr<ComponentConfiguration>>());

  bool CompareAndSetState(
    std::shared_ptr<ComponentManagerState>* expectedState,
    std::shared_ptr<ComponentManagerState> desiredState) override
  {
    bool superresult =
      ComponentManagerImpl::CompareAndSetState(expectedState, desiredState);
    statechangecount += superresult ? 1 : 0;
    return superresult;
  }

  void ResetCounter() { statechangecount = 0; }
  // count the number of successful state swaps. One successful state transition = 2 atomic state swaps
  std::atomic<int> statechangecount;
};

class MockComponentConfigurationImpl : public ComponentConfigurationImpl
{
public:
  MockComponentConfigurationImpl(
    std::shared_ptr<const metadata::ComponentMetadata> metadata,
    const Bundle& bundle,
    std::shared_ptr<const ComponentRegistry> registry,
    std::shared_ptr<cppmicroservices::logservice::LogService> logger)
    : ComponentConfigurationImpl(metadata, bundle, registry, logger)
    , statechangecount(0)
  {}
  virtual ~MockComponentConfigurationImpl() = default;
  MOCK_METHOD0(GetFactory, std::shared_ptr<ServiceFactory>(void));
  MOCK_METHOD1(
    CreateAndActivateComponentInstance,
    std::shared_ptr<ComponentInstance>(const cppmicroservices::Bundle&));
  MOCK_METHOD1(UnbindAndDeactivateComponentInstance,
               void(std::shared_ptr<ComponentInstance>));
  MOCK_METHOD0(DestroyComponentInstances, void());
  MOCK_METHOD2(BindReference,
               void(const std::string&, const ServiceReferenceBase&));
  MOCK_METHOD2(UnbindReference,
               void(const std::string&, const ServiceReferenceBase&));
  void SetState(const std::shared_ptr<ComponentConfigurationState>& newState)
  {
    ComponentConfigurationImpl::SetState(newState);
  }

  bool CompareAndSetState(
    std::shared_ptr<ComponentConfigurationState>* expectedState,
    std::shared_ptr<ComponentConfigurationState> desiredState) override
  {
    bool superresult = ComponentConfigurationImpl::CompareAndSetState(
      expectedState, desiredState);
    statechangecount += superresult ? 1 : 0;
    return superresult;
  }
  void ResetCounter() { statechangecount = 0; }
  // count the number of successful state swaps. One successful state transition = 2 atomic state swaps
  std::atomic<int> statechangecount;
};

}
}

#endif /* __MOCKS_HPP__ */
