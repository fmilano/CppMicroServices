#include "ServiceImpl.hpp"
#include <iostream>

namespace sample {
std::string TestBundleDSMyTest::Description()
{
  return STRINGIZE(US_BUNDLE_NAME);
}
}
