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

#ifndef TestFixture_h
#define TestFixture_h

#include "gtest/gtest.h"

#include <cppmicroservices/Bundle.h>
#include <cppmicroservices/BundleContext.h>
#include <cppmicroservices/BundleEvent.h>
#include <cppmicroservices/Framework.h>
#include <cppmicroservices/FrameworkEvent.h>
#include <cppmicroservices/FrameworkFactory.h>

#include "cppmicroservices/servicecomponent/ComponentConstants.hpp"
#include "cppmicroservices/servicecomponent/runtime/ServiceComponentRuntime.hpp"
#include "cppmicroservices/cm/ConfigurationAdmin.hpp"
#include "TestUtils.hpp"
#include <chrono>

namespace test {

auto const DEFAULT_POLL_PERIOD = std::chrono::milliseconds(1000);

namespace scr = cppmicroservices::service::component::runtime;

/**
 * Test Fixture used for several test points. The setup method installs and starts
 * all the bundles found in the compendium plugins folder and the installs all the
 * test bundles.
 * This class also provides convenience methods to start a specific test bundle 
 */
class tServiceComponent : public testing::Test
{
public:
  tServiceComponent()
    : ::testing::Test()
    , framework(cppmicroservices::FrameworkFactory().NewFramework())
  {}

  void SetUp() override
  {
    framework.Start();
    context = framework.GetBundleContext();

#if defined(US_BUILD_SHARED_LIBS)
    auto dsPluginPath = test::GetDSRuntimePluginFilePath();
    auto dsbundles = context.InstallBundles(dsPluginPath);
    for (auto& bundle : dsbundles) {
      bundle.Start();
    }
    
    auto caPluginPath = test::GetConfigAdminRuntimePluginFilePath();
    auto cabundles = context.InstallBundles(caPluginPath);
    for (auto& bundle : cabundles) {
      bundle.Start();
    }
    test::InstallLib(context, "DSFrenchDictionary");
    test::InstallLib(context, "EnglishDictionary");
    test::InstallLib(context, "TestBundleDSTOI1");
    test::InstallLib(context, "TestBundleDSTOI10");
    test::InstallLib(context, "TestBundleDSTOI12");
    test::InstallLib(context, "TestBundleDSTOI14");
    test::InstallLib(context, "TestBundleDSTOI15");
    test::InstallLib(context, "TestBundleDSTOI16");
    test::InstallLib(context, "TestBundleDSTOI2");
    test::InstallLib(context, "TestBundleDSTOI3");
    test::InstallLib(context, "TestBundleDSTOI5");
    test::InstallLib(context, "TestBundleDSTOI6");
    test::InstallLib(context, "TestBundleDSTOI7");
    test::InstallLib(context, "TestBundleDSTOI9");
    test::InstallLib(context, "TestBundleDSCA1");
    test::InstallLib(context, "TestBundleDSCA08");
    test::InstallLib(context, "TestBundleDSCA20");
#endif

#ifndef US_BUILD_SHARED_LIBS
    auto dsbundles = context.GetBundles();
    for (auto& bundle : dsbundles) {
      try {
        bundle.Start();
      } catch (std::exception& e) {
        std::cerr << "    " << e.what();
      }
      std::cerr << std::endl;
    }
#endif
    auto sRef = context.GetServiceReference<scr::ServiceComponentRuntime>();
    ASSERT_TRUE(sRef);
    dsRuntimeService = context.GetService<scr::ServiceComponentRuntime>(sRef);
    ASSERT_TRUE(dsRuntimeService);
  }

  void TearDown() override
  {
    framework.Stop();
    framework.WaitForStop(std::chrono::milliseconds::zero());
  }

  cppmicroservices::Bundle GetTestBundle(const std::string& symbolicName)
  {
    auto bundles = context.GetBundles();

    for (auto& bundle : bundles) {
      auto bundleSymbolicName = bundle.GetSymbolicName();
      if (symbolicName == bundleSymbolicName) {
        return bundle;
      }
    }
    return cppmicroservices::Bundle();
  }

  cppmicroservices::Bundle StartTestBundle(const std::string& symName)
  {
    cppmicroservices::Bundle testBundle = GetTestBundle(symName);
    EXPECT_EQ(static_cast<bool>(testBundle), true);
    testBundle.Start();
    EXPECT_EQ(testBundle.GetState(), cppmicroservices::Bundle::STATE_ACTIVE)
      << " failed to start bundle with symbolic name" + symName;
    return testBundle;
  }

  template<class T>
  std::shared_ptr<T> GetInstance()
  {
    cppmicroservices::ServiceReference<T> instanceRef;
    std::shared_ptr<T> instance;
    instanceRef = context.GetServiceReference<T>();
    if (!instanceRef) {
      return std::shared_ptr<T>();
    }
    return context.GetService<T>(instanceRef);
  }

  std::vector<scr::dto::ComponentConfigurationDTO> GetComponentConfigs(
    const cppmicroservices::Bundle& testBundle,
    const std::string& componentName,
    scr::dto::ComponentDescriptionDTO compDescDTO)
  {
    compDescDTO =
      dsRuntimeService->GetComponentDescriptionDTO(testBundle,
                                                   componentName);
    EXPECT_EQ(compDescDTO.implementationClass, componentName)
      << "Implementation class in the returned component description must be "
      << componentName;

    return dsRuntimeService->GetComponentConfigurationDTOs(compDescDTO);
  }

  std::shared_ptr<scr::ServiceComponentRuntime> dsRuntimeService;
  //std::shared_ptr<cppmicroservices::service::cm::ConfigurationAdmin>  configAdminService;
  cppmicroservices::Framework framework;
  cppmicroservices::BundleContext context;

};

}

#endif /* TestFixture_h */
