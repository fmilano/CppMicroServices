#ifndef _SERVICE_IMPL_HPP_
#define _SERVICE_IMPL_HPP_

#include "TestInterfaces/Interfaces.hpp"
#include "cppmicroservices/servicecomponent/ComponentContext.hpp"
#include <mutex>

using ComponentContext = cppmicroservices::service::component::ComponentContext;

namespace sample {
class TestBundleDSMyTest : public ::test::TestBundleDSMyTest
{
public:
  TestBundleDSMyTest(std::shared_ptr<void> unused = nullptr) {}

  std::string TestBundleDSMyTest::Description();

  ~TestBundleDSMyTest() = default;
};
}

#endif // _SERVICE_IMPL_HPP_
