#ifndef __SDR_TEST_BUFFERTEST_HH__
#define __SDR_TEST_BUFFERTEST_HH__

#include "buffer.hh"
#include "unittest.hh"


class BufferTest : public UnitTest::TestCase
{
public:
  virtual ~BufferTest();

  void testRefcount();
  void testReinterprete();
  void testRawRingBuffer();


public:
  static UnitTest::TestSuite *suite();
};

#endif // BUFFERTEST_HH
