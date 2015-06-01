#include "baudot.hh"
#include "traits.hh"
#include "logger.hh"


using namespace sdr;

// Baudot code tables
char Baudot::_letter[32] = {  0, 'E','\n', 'A', ' ', 'S', 'I', 'U','\n', 'D', 'R', 'J', 'N', 'F',
                            'C', 'K', 'T', 'Z', 'L', 'W', 'H', 'Y', 'P', 'Q', 'O', 'B', 'G',   0,
                            'M', 'X', 'V',  0};
char Baudot::_figure[32] = {  0, '3','\n', '-', ' ','\a', '8', '7','\n', '?', '4','\'', ',', '!',
                            ':', '(', '5', '"', ')', '2', '#', '6', '0', '1', '9', '?', '&',   0,
                            '.', '/', ';',  0};

// Some special codes
#define CHAR_NUL 0
#define CHAR_STF 27
#define CHAR_STL 31
#define CHAR_SPA 4


Baudot::Baudot(StopBits stopBits)
  : Sink<uint8_t>(), Source(), _mode(LETTERS)
{
  switch (stopBits) {
  case STOP1:
    // Pattern xx11 xxxx xxxx xx00
    // Mask    0011 0000 0000 0011
    _stopHBits     = 2;
    _bitsPerSymbol = 14;
    _pattern       = 0x3000;
    _mask          = 0x3003;
    break;
  case STOP15:
    // Pattern x11x xxxx xxxx x000
    // Mask    0110 0000 0000 0111
    _stopHBits     = 3;
    _bitsPerSymbol = 15;
    _pattern       = 0x6000;
    _mask          = 0x6007;
    break;
  case STOP2:
    // Pattern 11xx xxxx xxxx 0000
    // Mask    1100 0000 0000 1111
    _stopHBits     = 4;
    _bitsPerSymbol = 16;
    _pattern       = 0xC000;
    _mask          = 0xC00F;
    break;
  }
}

void
Baudot::config(const Config &src_cfg) {
  if (! src_cfg.hasType()) { return; }
  // Check if buffer type matches
  if (Config::typeId<uint8_t>() != src_cfg.type()) {
    ConfigError err;
    err << "Can not configure Baudot: Invalid type " << src_cfg.type()
        << ", expected " << Config::typeId<uint8_t>();
    throw err;
  }

  // Init (half) bit stream and counter
  _bitstream = 0;
  _bitcount  = 0;

  // Compute buffer size.
  size_t buffer_size = (src_cfg.bufferSize()/(2*_bitsPerSymbol))+1;
  _buffer  = Buffer<uint8_t>(buffer_size);

  LogMessage msg(LOG_DEBUG);
  msg << "Config Baudot node: " << std::endl
      << " input sample rate: " << src_cfg.sampleRate() << " half-bits/s" << std::endl
      << " start bits: " << 1 << std::endl
      << " stop bits: " << float(_stopHBits)/2 << std::endl;
  Logger::get().log(msg);

  // propergate config
  this->setConfig(Config(Traits<uint8_t>::scalarId, 0, buffer_size, 1));
}


void
Baudot::process(const Buffer<uint8_t> &buffer, bool allow_overwrite)
{
  size_t o=0;
  for (size_t i=0; i<buffer.size(); i++) {
    _bitstream = (_bitstream << 1) | (buffer[i] & 0x1); _bitcount++;
    // Check if symbol as received:
    if ((_bitsPerSymbol <= _bitcount) && (_pattern == (_bitstream & _mask))) {
      _bitcount = 0;
      // Unpack 5bit baudot code
      uint8_t code = 0;
      for (int j=0; j<5; j++) {
        int shift = _stopHBits + 2*j;
        code |= (((_bitstream>>shift)&0x01)<<j);
      }
      // Decode to ASCII
      if (CHAR_STL == code) { _mode = LETTERS; }
      else if (CHAR_STF == code) { _mode = FIGURES; }
      else {
        if (CHAR_SPA == code) { _mode = LETTERS; }
        if (LETTERS == _mode) { _buffer[o++] = _letter[code]; }
        else { _buffer[o++] = _figure[code]; }
      }
    }
  }
  if (0 < o) { this->send(_buffer.head(o)); }
}
