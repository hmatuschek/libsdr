#ifndef __SDT_TEST_CORETEST_HH__
#define __SDT_TEST_CORETEST_HH__

#include "unittest.hh"

class CoreTest : public UnitTest::TestCase
{
public:
  virtual ~CoreTest();

  void testShiftOperators();


public:
  static UnitTest::TestSuite *suite();
};

#endif // __SDT_TEST_CORETEST_HH__
