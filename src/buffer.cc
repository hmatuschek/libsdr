#include "buffer.hh"
#include <stdlib.h>

using namespace sdr;


/* ********************************************************************************************* *
 * Implementation of RawBuffer
 * ********************************************************************************************* */
RawBuffer::RawBuffer()
  : _ptr(0), _storage_size(0), _b_offset(0), _b_length(0), _refcount(0), _owner(0)
{
  // pass...
}

RawBuffer::RawBuffer(char *data, size_t offset, size_t len)
  : _ptr(data), _storage_size(offset+len), _b_offset(offset), _b_length(len),
    _refcount(0), _owner(0)
{
  // pass...
}

RawBuffer::RawBuffer(size_t N, BufferOwner *owner)
  : _ptr((char *)malloc(N)), _storage_size(N), _b_offset(0), _b_length(N),
    _refcount((int *)malloc(sizeof(int))), _owner(owner)
{
  // Check if data could be allocated
  if ((0 == _ptr) && (0 != _refcount)) {
    free(_refcount); _refcount = 0;  _storage_size = 0; return;
  }
  // Set refcount, done...
  if (_refcount) { (*_refcount) = 1; }
}

RawBuffer::RawBuffer(const RawBuffer &other)
  : _ptr(other._ptr), _storage_size(other._storage_size),
    _b_offset(other._b_offset), _b_length(other._b_length),
    _refcount(other._refcount), _owner(other._owner)
{
  // pass...
}

RawBuffer::RawBuffer(const RawBuffer &other, size_t offset, size_t len)
  : _ptr(other._ptr), _storage_size(other._storage_size),
    _b_offset(other._b_offset+offset), _b_length(len),
    _refcount(other._refcount), _owner(other._owner)
{
  // pass...
}

RawBuffer::~RawBuffer()
{
  // pass...
}

/** Increment reference counter. */
void RawBuffer::ref() const {
  if (0 != _refcount) { (*_refcount)++; }
}


/** Dereferences the buffer. */
void RawBuffer::unref() {
  // If empty -> skip...
  if ((0 == _ptr) || (0 == _refcount)) { return; }
  // Decrement refcount
  (*_refcount)--;
  // If there is only one reference left and the buffer is owned -> notify owner, who holds the last
  // reference.
  if ((1 == (*_refcount)) && (_owner)) { _owner->bufferUnused(*this); }
  // If the buffer is unreachable -> free
  if (0 == (*_refcount)) {
    free(_ptr); free(_refcount);
    // mark as empty
    _ptr = 0; _refcount=0;
  }
}



/* ********************************************************************************************* *
 * Implementation of RawRingBuffer
 * ********************************************************************************************* */
RawRingBuffer::RawRingBuffer()
  : RawBuffer(), _take_idx(0), _b_stored(0)
{
  // pass...
}

RawRingBuffer::RawRingBuffer(size_t size)
  : RawBuffer(size), _take_idx(0), _b_stored(0)
{
  // pass...
}

RawRingBuffer::RawRingBuffer(const RawRingBuffer &other)
  : RawBuffer(other), _take_idx(other._take_idx), _b_stored(other._b_stored)
{
  // pass...
}

RawRingBuffer::~RawRingBuffer() {
  // pass...
}



