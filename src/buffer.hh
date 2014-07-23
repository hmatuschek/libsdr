#ifndef __SDR_BUFFER_HH__
#define __SDR_BUFFER_HH__

#include <vector>
#include <map>
#include <list>
#include <ostream>
#include <complex>
#include <inttypes.h>
#include <cstdlib>
#include <cstring>

#include "config.hh"
#include "exception.hh"


namespace sdr {

// Forward declarations
class RawBuffer;

/** Abstract class (interface) of a buffer owner. If a buffer is owned, the owner gets notified
 * once the buffer gets unused. */
class BufferOwner {
public:
  /** Gets called once an owned buffer gets unused. */
  virtual void bufferUnused(const RawBuffer &buffer) = 0;
};


/** Base class of all buffers, represents an untyped array of bytes. */
class RawBuffer
{
public:
  /** Constructs an empty buffer. An empty buffer cannot be owned. */
  RawBuffer();

  /** Constructor from unowned data. */
  RawBuffer(char *data, size_t offset, size_t len);

  /** Constructs a buffer and allocates N bytes. */
  RawBuffer(size_t N, BufferOwner *owner=0);

  /** Copy constructor. */
  RawBuffer(const RawBuffer &other);

  /** Creates a new view on the buffer. */
  RawBuffer(const RawBuffer &other, size_t offset, size_t len);

  /** Destructor. */
  virtual ~RawBuffer();

  /** Assignment. */
  inline const RawBuffer &operator= (const RawBuffer &other) {
    // Now, reference new one
    _ptr = other._ptr;
    _storage_size = other._storage_size;
    _b_offset = other._b_offset;
    _b_length = other._b_length;
    _refcount = other._refcount;
    _owner = other._owner;
    // done.
    return *this;
  }

  /** Returns the pointer to the data (w/o view). */
  inline char *ptr() const { return (char *)_ptr; }
  /** Returns the pointer to the data of the buffer view. */
  inline char *data() const { return ((char *)_ptr+_b_offset); }
  /** Returns the offset of the data by the view. */
  inline size_t bytesOffset() const { return _b_offset; }
  /** Returns the size of the buffer by the view. */
  inline size_t bytesLen() const { return _b_length; }
  /** Returns the raw buffer size in bytes. */
  inline size_t storageSize() const { return _storage_size; }
  /** Returns true if the buffer is invalid/empty. */
  inline bool isEmpty() const { return 0 == _ptr; }

  /** Increment reference counter. */
  void ref() const;
  /** Dereferences the buffer. */
  void unref();
  /** Returns the reference counter. */
  inline int refCount() const { if (0 == _refcount) { return 0; } return (*_refcount); }
  /** We assume here that buffers are owned by some object: A buffer is therefore "unused" if the
   * owner holds the only reference to the buffer. */
  inline bool isUnused() const {
    if (0 == _refcount) { return true; }
    return (1 == (*_refcount));
  }

protected:
  /** Holds the pointer to the data or 0, if buffer is empty. */
  char *_ptr;
  /** Holds the size of the buffer in bytes. */
  size_t _storage_size;
  /** Holds the offset of the buffer in bytes. */
  size_t _b_offset;
  /** Holds the length of the buffer (view) in bytes. */
  size_t _b_length;
  /** The reference counter. */
  int *_refcount;
  /** Holds a weak reference the buffer owner. */
  BufferOwner *_owner;
};



/** A typed buffer. */
template <class T>
class Buffer: public RawBuffer
{
public:
  /** Empty constructor. */
  Buffer() : RawBuffer(), _size(0) {
    // pass...
  }

  /** Constructor from raw data. */
  Buffer(T *data, size_t size)
    : RawBuffer((char *)data, 0, sizeof(T)*size), _size(size) {
    // pass...
  }

  /** Creates a buffer with N samples. */
  Buffer(size_t N, BufferOwner *owner=0)
    : RawBuffer(N*sizeof(T), owner), _size(N) {
    // pass...
  }

  /** Create a new reference to the buffer. */
  Buffer(const Buffer<T> &other)
    : RawBuffer(other), _size(other._size) {
    // pass...
  }

  /** Destructor. */
  virtual ~Buffer() {
    _size = 0;
  }

  /** Explicit type cast. */
  explicit Buffer(const RawBuffer &other)
    : RawBuffer(other), _size(_b_length/sizeof(T))
  {
    // pass
  }


public:
  /** Assignment operator, turns this buffer into a reference to the @c other buffer. */
  const Buffer<T> &operator= (const Buffer<T> other) {
    RawBuffer::operator =(other);
    _size = other._size;
    return *this;
  }

  /** This is used to store buffers in sets. The comparison is performed on the pointer of the
   * buffers and not on their content. */
  inline bool operator<(const Buffer<T> &other) const {
    // Comparison by pointer to data
    return this->_ptr < other._ptr;
  }

  /** Returns the number of elements of type @c T in this buffer. */
  inline size_t size() const { return _size; }

  /** Element access. If compiled in debug mode, this function will perform a check on the
   * index and throws a @c RuntimeError exception if the index is out of bounds. */
  inline T &operator[] (int idx) const {
#ifdef SDR_DEBUG
    if ((idx >= _size) || (idx < 0)) {
      RuntimeError err;
      err << "Index " << idx << " out of bounds [0," << _size << ")";
      throw err;
    }
#endif
    return reinterpret_cast<T *>(_ptr+_b_offset)[idx];
  }

  /** Returns the \f$l^2\f$ norm of the buffer. */
  inline double norm2() const {
    double nrm2 = 0;
    for (size_t i=0; i<size(); i++) {
      nrm2 += std::real(std::conj((*this)[i])*(*this)[i]);
    }
    return std::sqrt(nrm2);
  }

  /** Returns the \f$l^1\f$ norm of the buffer. */
  inline double norm() const {
    double nrm = 0;
    for (size_t i=0; i<size(); i++) {
      nrm += std::abs((*this)[i]);
    }
    return nrm;
  }

  /** Returns the \f$l^p\f$ norm of the buffer. */
  inline double norm(double p) const {
    double nrm = 0;
    for (size_t i=0; i<size(); i++) {
      nrm += std::pow(std::abs((*this)[i]), p);
    }
    return std::pow(nrm, 1./p);
  }

  /** In-place, element wise product of the buffer with the scalar @c a. */
  inline Buffer<T> &operator *=(const T &a) {
    for (size_t i=0; i<size(); i++) {
      (*this)[i] *= a;
    }
    return *this;
  }

  /** In-place, element wise division of the buffer with the scalar @c a. */
  inline Buffer<T> &operator /=(const T &a) {
    for (size_t i=0; i<size(); i++) {
      (*this)[i] /= a;
    }
    return *this;
  }

  /** Explicit type cast. */
  template <class oT>
  Buffer<oT> as() const {
    return Buffer<oT>((const RawBuffer &)(*this));
  }

  /** Returns a new view on this buffer. */
  inline Buffer<T> sub(size_t offset, size_t len) const {
    if ((offset+len) > _size) { return Buffer<T>(); }
    return Buffer<T>(RawBuffer(*this, offset*sizeof(T), len*sizeof(T)));
  }

  /** Returns a new view on this buffer. */
  inline Buffer<T> head(size_t n) const {
    if (n > _size) { return Buffer<T>(); }
    return this->sub(0, n);
  }

  /** Returns a new view on this buffer. */
  inline Buffer<T> tail(size_t n) const {
    if (n > _size) { return Buffer<T>(); }
    return this->sub(_size-n, n);
  }

protected:
  /** Holds the number of elements of type T in the buffer. */
  size_t _size;
};

/** Pretty printing of a buffer. */
template <class Scalar>
std::ostream &
operator<< (std::ostream &stream, const sdr::Buffer<Scalar> &buffer)
{
  stream << "[";
  if (10 < buffer.size()) {
    stream << +buffer[0];
    for (size_t i=1; i<5; i++) {
      stream << ", " << +buffer[i];
    }
    stream << ", ..., " << +buffer[buffer.size()-6];
    for (size_t i=1; i<5; i++) {
      stream << ", " << +buffer[buffer.size()+i-6];
    }
  } else {
    if (0 < buffer.size()) {
      stream << +buffer[0];
      for (size_t i=1; i<buffer.size(); i++) {
        stream << ", " << +buffer[i];
      }
    }
  }
  stream << "]";
  return stream;
}


/** A set of buffers, that tracks their usage. Frequently it is impossible to predict the time, a
 * buffer will be in use. Instead of allocating a new buffer during runtime, one may allocate
 * several buffer in advance. In this case, it is important to track which buffer is still in use
 * efficiently. This class implements this functionality. A @c BufferSet pre-allocates several
 * buffers. Once a buffer is requested from the set, it gets marked as "in-use". Once the buffer
 * gets ununsed, it will be marked as "unused" and will be available again. */
template <class Scalar>
class BufferSet: public BufferOwner
{
public:
  /** Preallocates N buffers of size @c size. */
  BufferSet(size_t N, size_t size)
    : _bufferSize(size)
  {
    _free_buffers.reserve(N);
    for (size_t i=0; i<N; i++) {
      Buffer<Scalar> buffer(size, this);
      _buffers[buffer.ptr()] = buffer;
      _free_buffers.push_back(buffer.ptr());
    }
  }

  /** Destructor, unreferences all buffers. Buffers still in use, are freed once they are
   * dereferenced. */
  virtual ~BufferSet() {
    typename std::map<void *, Buffer<Scalar> >::iterator item = _buffers.begin();
    for(; item != _buffers.end(); item++) {
      item->second.unref();
    }
    _buffers.clear();
    _free_buffers.clear();
  }

  /** Returns true if there is a free buffer. */
  inline bool hasBuffer() { return _free_buffers.size(); }

  /** Obtains a free buffer. */
  inline Buffer<Scalar> getBuffer() {
    void *id = _free_buffers.back(); _free_buffers.pop_back();
    return _buffers[id];
  }

  /** Callback gets called once the buffer gets unused. */
  virtual void bufferUnused(const RawBuffer &buffer) {
    // Add buffer to list of free buffers if they are still owned
    if (0 != _buffers.count(buffer.ptr())) {
      _free_buffers.push_back(buffer.ptr());
    }
  }

  /** Resize the buffer set. */
  void resize(size_t numBuffers) {
    if (_buffers.size() == numBuffers) { return; }
    if (_buffers.size() < numBuffers) {
      // add some buffers
      size_t N = (numBuffers - _buffers.size());
      for (size_t i=0; i<N; i++) {
        Buffer<Scalar> buffer(_bufferSize, this);
        _buffers[buffer.ptr()] = buffer;
      }
    }
  }

protected:
  /** Size of each buffer. */
  size_t _bufferSize;
  /** Holds a reference to each buffer of the buffer set, referenced by the data pointer of the
   *  buffer. */
  std::map<void *, Buffer<Scalar> > _buffers;
  /** A vector of all unused buffers. */
  std::vector<void *> _free_buffers;
};


/** A simple ring buffer. */
class RawRingBuffer: public RawBuffer
{
public:
  /** Empty constructor. */
  RawRingBuffer();
  /** Constructs a raw ring buffer with size @c size. */
  RawRingBuffer(size_t size);
  /** Copy constructor. */
  RawRingBuffer(const RawRingBuffer &other);

  /** Destructor. */
  virtual ~RawRingBuffer();

  /** Assignment operator, turns this ring buffer into a reference to the other one. */
  inline const RawRingBuffer &operator=(const RawRingBuffer &other) {
    RawBuffer::operator =(other);
    _take_idx = other._take_idx;
    _b_stored = other._b_stored;
    return *this;
  }

  /** Element access. */
  char &operator[] (int idx) {
#ifdef SDR_DEBUG
    if ((idx < 0) || (idx>=(int)_b_stored)) {
      RuntimeError err;
      err << "RawRingBuffer: Index " << idx << " out of bounds [0," << bytesLen() << ").";
      throw err;
     }
#endif
    int i = _take_idx+idx;
    if (i >= (int)_storage_size) { i -= _storage_size; }
    return *(ptr()+i);
  }

  /** Returns the number of bytes available for reading. */
  inline size_t bytesLen() const { return _b_stored; }

  /** Returns the number of free bytes in the ring buffer. */
  inline size_t bytesFree() const {
    return _storage_size-_b_stored;
  }

  /** Puts the given data into the ring-buffer. The size of @c src must be smaller or equal to
   * the number of free bytes. Returns @c true if the data was written successfully to the buffer. */
  inline bool put(const RawBuffer &src) {
    /// @todo Allow overwrite
    // Data does not fit into ring buffer -> stop
    if (src.bytesLen() > bytesFree()) { return false; }
    // Obtain put_ptr
    size_t put_idx = _take_idx+_b_stored;
    if (put_idx > _storage_size) { put_idx -= _storage_size; }
    // store data
    if (_storage_size >= (put_idx+src.bytesLen())) {
      // If the data can be copied directly
      memcpy(_ptr+put_idx, src.data(), src.bytesLen());
      _b_stored += src.bytesLen();
      return true;
    }
    // If wrapped around -> Store first half
    size_t num_a = _storage_size-put_idx;
    memcpy(_ptr+put_idx, src.data(), num_a);
    // store second half
    memcpy(_ptr, src.data()+num_a, src.bytesLen()-num_a);
    _b_stored += src.bytesLen();
    return true;
  }

  /** Take @c N bytes from the ring buffer and store it into the given buffer @c dest.
   * Returns @c true if the data was taken successfully from the ring buffer. */
  inline bool take(const RawBuffer &dest, size_t N) {
    // If there is not enough space in dest to store N bytes
    if (N > dest.bytesLen()) { return false; }
    // If there is not enough data available
    if (N > bytesLen()) { return false; }
    // If data can be taken at once
    if (_storage_size > (_take_idx+N)) {
      memcpy(dest.data(), _ptr+_take_idx, N);
      _take_idx += N; _b_stored -= N;
      return true;
    }
    // Copy in two steps
    size_t num_a = _storage_size-_take_idx;
    memcpy(dest.data(), _ptr+_take_idx, num_a);
    memcpy(dest.data()+num_a, _ptr, N-num_a);
    _take_idx = N-num_a; _b_stored -= N;
    return true;
  }

  /** Drops at most @c N bytes from the buffer. */
  inline void drop(size_t N) {
    N = std::min(N, bytesLen());
    if (_storage_size>(_take_idx+N)) { _take_idx+=N; _b_stored -= N; }
    else { _take_idx = N-(_storage_size-_take_idx); _b_stored -= N; }
  }

  /** Clear the ring-buffer. */
  inline void clear() { _take_idx = _b_stored = 0; }

  /** Resizes the ring buffer. Also clears it. */
  inline void resize(size_t N) {
    if (_storage_size == N) { return; }
    _take_idx=_b_stored=0;
    RawBuffer::operator =(RawBuffer(N));
  }

protected:
  /** The current read pointer. */
  size_t _take_idx;
  /** Offset of the write pointer relative to the @c _take_idx ptr, such that the number of
   * stored bytes is @c _b_stored and the write index is (_take_idx+_b_stored) % _storage_size. */
  size_t _b_stored;
};


/** A simple typed ring-buffer. */
template <class Scalar>
class RingBuffer: public RawRingBuffer
{
public:
  /** Empty constructor. */
  RingBuffer() : RawRingBuffer(), _size(0), _stored(0) { }

  /** Constructs a ring buffer of size @c N. */
  RingBuffer(size_t N) : RawRingBuffer(N*sizeof(Scalar)), _size(N), _stored(0) { }

  /** Copy constructor, creates a reference to the @c other ring buffer. */
  RingBuffer(const RingBuffer<Scalar> &other)
    : RawRingBuffer(other), _size(other._size), _stored(other._stored) { }

  /** Destructor. */
  virtual ~RingBuffer() { }

  /** Assigment operator, turns this buffer into a reference to the @c other ring buffer. */
  const RingBuffer<Scalar> &operator= (const RingBuffer<Scalar> &other) {
    RawRingBuffer::operator =(other);
    _size = other._size; _stored = other._stored;
    return *this;
  }

  /** Element access. */
  Scalar &operator[] (int idx) {
    return reinterpret_cast<Scalar &>(RawRingBuffer::operator [](idx*sizeof(Scalar)));
  }

  /** Returns the number of stored elements. */
  inline size_t stored() const { return _stored; }
  /** Returns the number of free elements. */
  inline size_t free() const { return _size-_stored; }
  /** Returns the size of the ring buffer. */
  inline size_t size() const { return _size; }

  /** Puts the given elements into the ring buffer. If the elements do not fit into the
   *  buffer, @c false is returned. */
  inline bool put(const Buffer<Scalar> &data) {
    if (RawRingBuffer::put(data)) {
      _stored += data.size(); return true;
    }
    return false;
  }

  /** Takes @c N elements from the buffer and stores them into @c dest. Returns @c true
   * on success. */
  inline bool take(const Buffer<Scalar> &dest, size_t N) {
    if (RawRingBuffer::take(dest, N*sizeof(Scalar))) {
      _stored -= N; return true;
    }
    return false;
  }

  /** Drops @c N elements from the ring buffer. */
  inline void drop(size_t N) {
    RawRingBuffer::drop(N*sizeof(Scalar)); _stored = _b_stored/sizeof(Scalar);
  }

  /** Resizes the ring buffer to @c N elements. */
  inline void resize(size_t N) {
    RawRingBuffer::resize(N*sizeof(Scalar));
  }

protected:
  /** The size of the ring buffer. */
  size_t _size;
  /** The number of stored elements. */
  size_t _stored;
};

}

#endif // __SDR_BUFFER_HH__
