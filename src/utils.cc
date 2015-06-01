#include "utils.hh"

using namespace sdr;


/* ********************************************************************************************* *
 * Implementation of UnsignedToSinged
 * ********************************************************************************************* */
UnsignedToSigned::UnsignedToSigned(float scale)
  : SinkBase(), Source(), _scale(scale)
{
  // pass...
}

UnsignedToSigned::~UnsignedToSigned() {
  // pass...
}

void
UnsignedToSigned::config(const Config &src_cfg) {
  // Requires type
  if (!src_cfg.hasType()) { return; }

  size_t scalar_size;
  Config::Type out_type;
  // Requires a unsigned integer or complex unsigned integer type as input
  switch (src_cfg.type()) {
  case Config::Type_u8:
    scalar_size = 1; out_type = Config::Type_s8;
    _process = &UnsignedToSigned::_process_int8;
    break;
  case Config::Type_cu8:
    scalar_size = 2; out_type = Config::Type_cs8;
    _process = &UnsignedToSigned::_process_int8;
    break;
  case Config::Type_u16:
    scalar_size = 2; out_type = Config::Type_s16;
    _process = &UnsignedToSigned::_process_int16;
    break;
  case Config::Type_cu16:
    scalar_size = 4; out_type = Config::Type_cs16;
    _process = &UnsignedToSigned::_process_int16;
    break;

  default: {
    ConfigError err;
    err << "Can not configure Unsigned2Signed node: Invalid input type " << src_cfg.type()
        << ", expected " << Config::Type_u8 << ", " << Config::Type_cu8 << ", " << Config::Type_u16
        << " or " << Config::Type_cu8;
    throw err;
   }
  }

  // unreference buffer if non-empty
  if (!_buffer.isEmpty()) { _buffer.unref(); }
  // Allocate buffer
  _buffer = RawBuffer(scalar_size*src_cfg.bufferSize());
  // Propergate config
  this->setConfig(Config(
                    out_type, src_cfg.sampleRate(), src_cfg.bufferSize(), 1));
}

void
UnsignedToSigned::handleBuffer(const RawBuffer &buffer, bool allow_overwrite) {
  if (allow_overwrite) {
    (this->*_process)(buffer, buffer);
  }
  else if (_buffer.isUnused()) {
    (this->*_process)(buffer, _buffer);
  } else {
#ifdef SDR_DEBUG
    std::cerr << "Unsigned2Signed: Drop buffer: Output buffer still in use." << std::endl;
#endif
  }
}

void
UnsignedToSigned::_process_int8(const RawBuffer &in, const RawBuffer &out) {
  size_t num = in.bytesLen();
  uint8_t *in_ptr = (uint8_t *) in.data();
  int8_t *out_ptr = (int8_t *) out.data();
  //std::cerr << "UnsingedToSigned: Process " << Buffer<uint8_t>(in) << std::endl;
  for (size_t i=0; i<num; i++, in_ptr++, out_ptr++) {
    *out_ptr = int16_t(*in_ptr) - 128;
  }
  //std::cerr << " -> " << Buffer<int8_t>(RawBuffer(out, 0, num)) << std::endl;
  this->send(RawBuffer(out, 0, num), true);
}

void
UnsignedToSigned::_process_int16(const RawBuffer &in, const RawBuffer &out) {
  size_t num = in.bytesLen()/2;
  uint16_t *in_ptr = (uint16_t *) in.data();
  int16_t *out_ptr = (int16_t *) out.data();
  for (size_t i=0; i<num; i++, in_ptr++, out_ptr++) {
    *out_ptr = int32_t(*in_ptr) - 32768;
  }
  this->send(RawBuffer(out, 0, num), true);
}



/* ********************************************************************************************* *
 * Implementation of SignedToUnsinged
 * ********************************************************************************************* */
SignedToUnsigned::SignedToUnsigned()
  : SinkBase(), Source()
{
  // pass...
}

SignedToUnsigned::~SignedToUnsigned() {
  // pass...
}

void
SignedToUnsigned::config(const Config &src_cfg) {
  // Requires type
  if (!src_cfg.hasType()) { return; }

  size_t scalar_size;
  Config::Type out_type;
  // Requires a unsigned integer or complex unsigned integer type as input
  switch (src_cfg.type()) {
  case Config::Type_s8:
    scalar_size = 1; out_type = Config::Type_u8;
    _process = &SignedToUnsigned::_process_int8;
    break;
  case Config::Type_cs8:
    scalar_size = 2; out_type = Config::Type_cu8;
    _process = &SignedToUnsigned::_process_int8;
    break;
  case Config::Type_s16:
    scalar_size = 2; out_type = Config::Type_u16;
    _process = &SignedToUnsigned::_process_int16;
    break;
  case Config::Type_cs16:
    scalar_size = 4; out_type = Config::Type_cu16;
    _process = &SignedToUnsigned::_process_int16;
    break;

  default: {
    ConfigError err;
    err << "Can not configure SignedToUnsigned node: Invalid input type " << src_cfg.type()
        << ", expected " << Config::Type_s8 << ", " << Config::Type_cs8 << ", " << Config::Type_s16
        << " or " << Config::Type_cs8;
    throw err;
   }
  }

  // Allocate buffer
  _buffer = RawBuffer(scalar_size*src_cfg.bufferSize());
  // Propergate config
  this->setConfig(Config(out_type, src_cfg.sampleRate(), src_cfg.bufferSize(), 1));
}

void
SignedToUnsigned::handleBuffer(const RawBuffer &buffer, bool allow_overwrite) {
  if (allow_overwrite) {
    (this->*_process)(buffer, buffer);
  }
  else if (_buffer.isUnused()) {
    (this->*_process)(buffer, _buffer);
  } else {
#ifdef SDR_DEBUG
    std::cerr << "SignedToUnsigned: Drop buffer: Output buffer still in use." << std::endl;
#endif
  }
}

void
SignedToUnsigned::_process_int8(const RawBuffer &in, const RawBuffer &out) {
  size_t num = in.bytesLen();
  for (size_t i=0; i<num; i++) {
    ((uint8_t *)out.data())[i] = int(((int8_t *)in.data())[i]) + 128;
  }
  this->send(RawBuffer(out, 0, num), true);
}

void
SignedToUnsigned::_process_int16(const RawBuffer &in, const RawBuffer &out) {
  size_t num = in.bytesLen()/2;
  for (size_t i=0; i<num; i++) {
    ((uint16_t *)out.data())[i] = int32_t( ((int16_t *)in.data())[i] ) + 32768;
  }
  this->send(RawBuffer(out, 0, num), true);
}


/* ********************************************************************************************* *
 * Implementation of TextDump
 * ********************************************************************************************* */
TextDump::TextDump(std::ostream &stream)
  : Sink<uint8_t>(), _stream(stream)
{
  // pass...
}

void
TextDump::config(const Config &src_cfg) {
  // Requires type
  if (!src_cfg.hasType()) { return; }
  if (src_cfg.type() != Traits<uint8_t>::scalarId) {
    ConfigError err;
    err << "Can not configure TextDump node: Invalid input type " << src_cfg.type()
        << ", expected " << Config::Type_u8 << ".";
    throw err;
  }
  // done
}

void
TextDump::process(const Buffer<uint8_t> &buffer, bool allow_overwrite) {
  for (size_t i=0; i<buffer.size(); i++) {
    _stream << char(buffer[i]);
  }
}

