#include "coretest.hh"
#include "coreutilstest.hh"
#include "unittest.hh"
#include "buffertest.hh"
#include <iostream>

using namespace sdr;


int main(int argc, char *argv[]) {

  UnitTest::TestRunner runner(std::cout);

  runner.addSuite(CoreTest::suite());
  runner.addSuite(BufferTest::suite());
  runner.addSuite(CoreUtilsTest::suite());

  runner();

  return 0;
}
