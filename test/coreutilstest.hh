#ifndef __SDR_TEST_COREUTILSTEST_HH__
#define __SDR_TEST_COREUTILSTEST_HH__

#include "unittest.hh"

class CoreUtilsTest : public UnitTest::TestCase
{
public:
  virtual ~CoreUtilsTest();

  void testUChar2Char();
  void testUShort2Short();
  void testInterleave();

public:
  static UnitTest::TestSuite *suite();
};

#endif
