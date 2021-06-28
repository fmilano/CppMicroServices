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
#ifndef CPPMICROSERVICES_ASYNC_WORK_SERVICE_H__
#define CPPMICROSERVICES_ASYNC_WORK_SERVICE_H__

#include "cppmicroservices/asyncworkservice/AsyncWorkServiceExport.h"

#include "cppmicroservices/ServiceReferenceBase.h"

#include <cstdint>
#include <exception>
#include <future>
#include <string>

namespace cppmicroservices {
namespace async {
namespace detail {

class US_usAsyncWorkService_EXPORT AsyncWorkService
{
public:
  virtual ~AsyncWorkService();

  virtual std::future<void> post(std::function<void()>&& task) = 0;

  virtual void post(std::packaged_task<void()>&& task) = 0;
};

} // namespace detail

} // namespace async

} // namespace cppmicroservices

#endif // CPPMICROSERVICES_ASYNC_WORK_SERVICE_H__
