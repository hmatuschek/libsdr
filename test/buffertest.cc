#include "buffertest.hh"
#include <iostream>
using namespace sdr;
using namespace UnitTest;

BufferTest::~BufferTest() { }


void
BufferTest::testRefcount() {
  Buffer<int8_t> a(3);

  // Test Direct reference counting
  UT_ASSERT_EQUAL(a.refCount(), 1);
  UT_ASSERT(a.isUnused());

  {
    Buffer<int8_t> b(a);
    UT_ASSERT_EQUAL(a.refCount(), 1);
    UT_ASSERT_EQUAL(b.refCount(), 1);
    UT_ASSERT(a.isUnused());
    UT_ASSERT(b.isUnused());
  }

  {
    Buffer<int8_t> b(a);
    b.ref();
    UT_ASSERT_EQUAL(a.refCount(), 2);
    UT_ASSERT_EQUAL(b.refCount(), 2);
    UT_ASSERT(!a.isUnused());
    UT_ASSERT(!b.isUnused());
    b.unref();
  }

  UT_ASSERT_EQUAL(a.refCount(), 1);
  UT_ASSERT(a.isUnused());

  // Test indirect reference counting
  std::list<RawBuffer> buffers;
  buffers.push_back(a);

  UT_ASSERT_EQUAL(a.refCount(), 1);
  UT_ASSERT(a.isUnused());

  // check direct referenceing
  UT_ASSERT_EQUAL(buffers.back().refCount(), 1);
  UT_ASSERT(buffers.back().isUnused());

  buffers.pop_back();
  UT_ASSERT_EQUAL(a.refCount(), 1);
  UT_ASSERT(a.isUnused());
}


void
BufferTest::testReinterprete() {
  // Check handle interleaved numbers as real & imag part
  Buffer<int8_t> real_buffer(4);

  real_buffer[0] = 1;
  real_buffer[1] = 2;
  real_buffer[2] = 3;
  real_buffer[3] = 4;

  // Cast to complex char
  Buffer< std::complex<int8_t> >  cmplx_buffer(real_buffer);
  // Check size
  UT_ASSERT_EQUAL(real_buffer.size()/2, cmplx_buffer.size());
  // Check content
  UT_ASSERT_EQUAL(cmplx_buffer[0], std::complex<int8_t>(1,2));
  UT_ASSERT_EQUAL(cmplx_buffer[1], std::complex<int8_t>(3,4));
}


void
BufferTest::testRawRingBuffer() {
  RawBuffer a(3), b(3);
  RawRingBuffer ring(3);

  memcpy(a.data(), "abc", 3);

  // Check if ring is empty
  UT_ASSERT_EQUAL(ring.bytesLen(), size_t(0));
  UT_ASSERT_EQUAL(ring.bytesFree(), size_t(3));

  // Put a byte
  UT_ASSERT(ring.put(RawBuffer(a, 0, 1)));
  UT_ASSERT_EQUAL(ring.bytesLen(), size_t(1));
  UT_ASSERT_EQUAL(ring.bytesFree(), size_t(2));

  // Put two more bytes
  UT_ASSERT(ring.put(RawBuffer(a, 1, 2)));
  UT_ASSERT_EQUAL(ring.bytesLen(), size_t(3));
  UT_ASSERT_EQUAL(ring.bytesFree(), size_t(0));

  // Now, the ring is full, any further put should fail
  UT_ASSERT(!ring.put(a));

  // Take a byte from ring
  UT_ASSERT(ring.take(b, 1));
  UT_ASSERT_EQUAL(ring.bytesLen(), size_t(2));
  UT_ASSERT_EQUAL(ring.bytesFree(), size_t(1));
  UT_ASSERT_EQUAL(*(b.data()), 'a');

  // Take another byte
  UT_ASSERT(ring.take(b, 1));
  UT_ASSERT_EQUAL(ring.bytesLen(), size_t(1));
  UT_ASSERT_EQUAL(ring.bytesFree(), size_t(2));
  UT_ASSERT_EQUAL(*(b.data()), 'b');

  // Put two more back
  UT_ASSERT(ring.put(RawBuffer(a, 0, 2)));
  UT_ASSERT_EQUAL(ring.bytesLen(), size_t(3));
  UT_ASSERT_EQUAL(ring.bytesFree(), size_t(0));

  // Take all
  UT_ASSERT(ring.take(b, 3));
  UT_ASSERT_EQUAL(ring.bytesLen(), size_t(0));
  UT_ASSERT_EQUAL(ring.bytesFree(), size_t(3));
  UT_ASSERT(0 == memcmp(b.data(), "cab", 3));

}


TestSuite *
BufferTest::suite() {
  TestSuite *suite = new TestSuite("Buffer Tests");

  suite->addTest(new TestCaller<BufferTest>(
                   "reference counter", &BufferTest::testRefcount));
  suite->addTest(new TestCaller<BufferTest>(
                   "re-interprete case", &BufferTest::testReinterprete));
  suite->addTest(new TestCaller<BufferTest>(
                   "raw ring buffer", &BufferTest::testRawRingBuffer));
  return suite;
}
