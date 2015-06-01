#ifndef __SDR_BUFFERNODE_HH__
#define __SDR_BUFFERNODE_HH__

#include "node.hh"
#include "config.hh"
#include "logger.hh"

#include <iostream>
#include <cstring>


namespace sdr {

/** A simple buffering node, that ensures a fixed buffer size. This node is useful, expecially in
 * front of a node that performs a FFT transform, which requires a certain buffer size.
 * @ingroup datanodes */
template <class Scalar>
class BufferNode : public Sink<Scalar>, public Source
{
public:
  /** Constructs a new buffer node.
   * @param bufferSize Specifies the desired size of the output buffers. */
  BufferNode(size_t bufferSize)
    : _bufferSize(bufferSize), _bufferSet(0, _bufferSize), _temp(bufferSize), _samplesLeft(0)
  {
    // pass...
  }

  /** Configures the buffer node. */
  virtual void config(const Config &src_cfg)
  {
    // Check if source config is complete
    if (Config::Type_UNDEFINED == src_cfg.type()) { return; }
    if (0 == src_cfg.bufferSize()) { return; }
    if (0 == src_cfg.numBuffers()) { return; }
    // Check source type
    if (src_cfg.type() != Config::typeId<Scalar>()) {
      ConfigError err;
      err << "Can not configure BufferNode sink. Source type is " << src_cfg.type()
          << " expected " << Config::typeId<Scalar>() << std::endl;
      throw err;
    }
    // Estimate number of buffers needed:
    size_t totSize = src_cfg.bufferSize()*src_cfg.numBuffers();
    size_t numBuffers = std::max(size_t(2), totSize/_bufferSize);
    _bufferSet.resize(numBuffers);

    LogMessage msg(LOG_DEBUG);
    msg << "Configure BufferNode: " << std::endl
        << " type: " << src_cfg.type() << std::endl
        << " sample-rate: " << src_cfg.sampleRate() << std::endl
        << " buffer-size: " << src_cfg.bufferSize()
        << " -> " << _bufferSize << std::endl
        << " # buffers: " << src_cfg.numBuffers();

    // Propergate source config
    this->setConfig(Config(src_cfg.type(), src_cfg.sampleRate(), _bufferSize, numBuffers));
  }

  /** Process the incomming data. */
  virtual void process(const Buffer<Scalar> &buffer, bool allow_overwrite)
  {
    // If the current buffer buffer does not contain enough smaples to fill an output buffer:
    if ((_samplesLeft+buffer.size()) < _bufferSize) {
      memcpy(_temp.data()+_samplesLeft*sizeof(Scalar), buffer.data(), sizeof(Scalar)*buffer.size());
      _samplesLeft += buffer.size();
      return;
    }
    // There are enough samples collected to fill an ouput buffer,
    // fill first out buffer and send it
    Buffer<Scalar> out = _bufferSet.getBuffer();
    memcpy(out.data(), _temp.data(), sizeof(Scalar)*_samplesLeft);
    memcpy(out.data()+_samplesLeft*sizeof(Scalar), buffer.data(), sizeof(Scalar)*(_bufferSize-_samplesLeft));
    size_t in_offset = (_bufferSize-_samplesLeft);

    // Determine the number of samples left
    _samplesLeft = buffer.size()+_samplesLeft-_bufferSize;
    this->send(out);

    // Process remaining data
    while (_samplesLeft >= _bufferSize) {
      Buffer<Scalar> out = _bufferSet.getBuffer();
      memcpy(out.data(), buffer.data()+in_offset*sizeof(Scalar), _bufferSize*sizeof(Scalar));
      in_offset += _bufferSize;
      _samplesLeft -= _bufferSize;
      this->send(out);
    }

    // Store data left over into temp
    memcpy(_temp.data(), buffer.data(), _samplesLeft*sizeof(Scalar));
  }

protected:
  /** The desired buffer size. */
  size_t _bufferSize;
  /** A set of output buffers. */
  BufferSet<Scalar> _bufferSet;
  /** An intermediate buffer to hold left-over samples from the previous buffers. */
  Buffer<Scalar> _temp;
  /** Number of samples left. */
  size_t _samplesLeft;
};

}

#endif // __SDR_BUFFERNODE_HH__
