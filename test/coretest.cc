#include "coretest.hh"
#include "sdr.hh"

using namespace sdr;


CoreTest::~CoreTest() { /* pass... */ }


void
CoreTest::testShiftOperators() {
  // Test if shift can be used as multiplication or division by a power of two
  // (even on negative integers)
  int a=128, b=-128;
  // On positive integers (should work always)
  UT_ASSERT_EQUAL(a>>1, 64);
  UT_ASSERT_EQUAL(a<<1, 256);
  UT_ASSERT_EQUAL(a>>0, 128);
  UT_ASSERT_EQUAL(a<<0, 128);

  UT_ASSERT_EQUAL(b>>1,  -64);
  UT_ASSERT_EQUAL(b<<1, -256);
  UT_ASSERT_EQUAL(b>>0, -128);
  UT_ASSERT_EQUAL(b<<0, -128);
}



UnitTest::TestSuite *
CoreTest::suite() {
  UnitTest::TestSuite *suite = new UnitTest::TestSuite("Core operations");

  suite->addTest(new UnitTest::TestCaller<CoreTest>(
                   "shift operators", &CoreTest::testShiftOperators));

  return suite;
}
