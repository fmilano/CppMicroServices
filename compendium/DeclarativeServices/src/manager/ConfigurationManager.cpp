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

#include "ConfigurationManager.hpp"

namespace cppmicroservices {
namespace scrimpl {
ConfigurationManager::ConfigurationManager(
  const std::shared_ptr<const metadata::ComponentMetadata> metadata,
  const cppmicroservices::BundleContext& bc,
    std::shared_ptr<cppmicroservices::logservice::LogService> logger)
  : metadata(metadata)
  , logger(std::move(logger))
  , bundleContext(bc)
  , mergedProperties (metadata->properties)
{
  if (!this->metadata || !this->bundleContext || !this->logger) {
    throw std::invalid_argument(
      "ConfigurationManagerImpl - Invalid arguments passed to constructor");
  }
}

cppmicroservices::AnyMap ConfigurationManager::GetProperties() const noexcept
{
  return mergedProperties;
}

void ConfigurationManager::Initialize() {
  if (metadata->configurationPids.empty() || metadata->configurationPolicy == "ignore") return;
  auto sr =
    this->bundleContext
      .GetServiceReference<cppmicroservices::service::cm::ConfigurationAdmin>();
  auto configAdmin =
    this->bundleContext
      .GetService<cppmicroservices::service::cm::ConfigurationAdmin>(sr);
  
  for (auto& pid : metadata->configurationPids) {
    if (configProperties.find(pid) == configProperties.end()) {
      auto config = configAdmin->ListConfigurations("(pid = " + pid + ")");
      if (config.size() > 0) {
        auto properties = config.front()->GetProperties();
        auto it = configProperties.find(pid);
        if (it != configProperties.end()) {
          configProperties.erase(it);
        }
        configProperties.emplace(pid, properties);
        for (auto item : properties) {
          mergedProperties[item.first] = item.second;
        }   
      }      
    } 
  }  
}

void ConfigurationManager::UpdateMergedProperties(const std::string pid, 
  std::shared_ptr<cppmicroservices::AnyMap> props,
  const cppmicroservices::service::cm::ConfigurationEventType type) noexcept
{
  // delete properties for this pid or replace with new properties in configProperties
  
  auto it = configProperties.find(pid);
  if (it != configProperties.end()) {
    configProperties.erase(it);
  }
  if (type == cppmicroservices::service::cm::ConfigurationEventType::CM_UPDATED) {
      configProperties.emplace(pid, *props);
  }
  
  //  recalculate the merged properties maintaining precedence as follows:
  //  lowest precedence service properties
  //  next precedence each pid in meta-data configuration-pids with first one
  //  in the list having lower precedence than the last one in the list. 

  mergedProperties = metadata->properties;

  for (auto& pid : metadata->configurationPids) {
    auto it = configProperties.find(pid);
    if (it != configProperties.end()) { 
       for (auto item : it->second) {
          mergedProperties[item.first] = item.second;
        }
      }
  } 
}

/**
   * Returns \c true if the configuration dependencies are satisfied, \c false otherwise
   */
bool ConfigurationManager::IsConfigSatisfied(
  const ComponentState currentState) const noexcept
{
    bool allConfigsAvailable =
    configProperties.size() >= metadata->configurationPids.size();

   if ((metadata->configurationPolicy == "ignore")
      || (allConfigsAvailable)) {
        return true;
   }
  
  if ((metadata->configurationPolicy == "require") 
      || ((metadata->configurationPolicy == "optional") &&
        (currentState == ComponentState::ACTIVE)))
  {
    return false;
  }
  
 return true;
  
}
void ConfigurationManager::SendModifiedPropertiesToComponent() {
    // see sequence diagram ConfigurationListener::configurationEvent(CM_UPDATED)

    // if component has a Modified method, call it with mergedProperties. 
    // if component does not have a Modified method, Deactivate and Reactivate the component.
}


}
}