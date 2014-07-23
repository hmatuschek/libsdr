#include "coreutilstest.hh"
#include "config.hh"
#include "utils.hh"
#include "combine.hh"

using namespace sdr;
using namespace UnitTest;

CoreUtilsTest::~CoreUtilsTest() { }

void
CoreUtilsTest::testUChar2Char() {
  Buffer<uint8_t> uchar_buffer(3);
  uchar_buffer[0] = 0u;
  uchar_buffer[1] = 128u;
  uchar_buffer[2] = 255u;

  // Assemble cast instance and configure it
  UnsignedToSigned cast;
  cast.config(Config(Config::Type_u8, 1, 3, 1));
  // Perform in-place operation
  cast.handleBuffer(uchar_buffer, true);

  // Reinterprete uchar buffer as char buffer
  Buffer<int8_t> char_buffer(uchar_buffer);
  // Check values
  UT_ASSERT_EQUAL(char_buffer[0], (signed char)-128);
  UT_ASSERT_EQUAL(char_buffer[1], (signed char)0);
  UT_ASSERT_EQUAL(char_buffer[2], (signed char)127);
}

void
CoreUtilsTest::testUShort2Short() {
  Buffer<uint16_t> uchar_buffer(3);
  uchar_buffer[0] = 0u;
  uchar_buffer[1] = 128u;
  uchar_buffer[2] = 255u;

  // Assemble cast instance and configure it
  UnsignedToSigned cast;
  cast.config(Config(Config::Type_u16, 1, 3, 1));
  // Perform in-place operation
  cast.handleBuffer(uchar_buffer, true);

  // Reinterprete uchar buffer as char buffer
  Buffer<int16_t> char_buffer(uchar_buffer);
  // Check values
  UT_ASSERT_EQUAL(char_buffer[0], (int16_t)-128);
  UT_ASSERT_EQUAL(char_buffer[1], (int16_t)0);
  UT_ASSERT_EQUAL(char_buffer[2], (int16_t)127);
}


void
CoreUtilsTest::testInterleave() {
  Interleave<int16_t> interl(2);
  DebugStore<int16_t> sink;
  Buffer<int16_t> a(3);

  interl.sink(0)->config(Config(Config::Type_s16, 1, a.size(), 1));
  interl.sink(1)->config(Config(Config::Type_s16, 1, a.size(), 1));
  interl.connect(&sink, true);

  // Send some data
  a[0] = 1;  a[1] = 2; a[2] = 3; interl.sink(0)->process(a, false);
  a[0] = 4;  a[1] = 5; a[2] = 6; interl.sink(1)->process(a, false);

  // Check content of sink
  UT_ASSERT_EQUAL(sink.buffer()[0], (int16_t)1);
  UT_ASSERT_EQUAL(sink.buffer()[1], (int16_t)4);
  UT_ASSERT_EQUAL(sink.buffer()[2], (int16_t)2);
  UT_ASSERT_EQUAL(sink.buffer()[3], (int16_t)5);
  UT_ASSERT_EQUAL(sink.buffer()[4], (int16_t)3);
  UT_ASSERT_EQUAL(sink.buffer()[5], (int16_t)6);
}


TestSuite *
CoreUtilsTest::suite() {
  TestSuite *suite = new TestSuite("Core Utils");

  suite->addTest(new TestCaller<CoreUtilsTest>(
                   "cast uint8_t -> int8_t", &CoreUtilsTest::testUChar2Char));
  suite->addTest(new TestCaller<CoreUtilsTest>(
                   "cast uint16_t -> int16_t", &CoreUtilsTest::testUChar2Char));
  suite->addTest(new TestCaller<CoreUtilsTest>(
                   "Interleave", &CoreUtilsTest::testInterleave));

  return suite;
}
