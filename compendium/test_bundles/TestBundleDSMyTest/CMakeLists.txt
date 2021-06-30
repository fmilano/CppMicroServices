usFunctionCreateDSTestBundle(TestBundleDSMyTest)

usFunctionCreateTestBundleWithResources(TestBundleDSMyTest
  SOURCES src/ServiceImpl.cpp ${_glue_file}
  RESOURCES manifest.json
  BUNDLE_SYMBOLIC_NAME TestBundleDSMyTest
  OTHER_LIBRARIES usTestInterfaces usServiceComponent)
