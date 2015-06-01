#include "ax25.hh"
#include "logger.hh"
#include "traits.hh"

using namespace sdr;


static const uint16_t crc_ccitt_table[] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

static inline bool check_crc_ccitt(const uint8_t *buf, int cnt)
{
    uint32_t crc = 0xffff;
    for (; cnt > 0; cnt--, buf++) {
        crc = (crc >> 8) ^ crc_ccitt_table[(crc ^ (*buf)) & 0xff];
    }
    return (crc & 0xffff) == 0xf0b8;
}



AX25::AX25()
  : Sink<uint8_t>(), Source()
{
  // pass...
}

AX25::~AX25() {
  // pass...
}

void
AX25::config(const Config &src_cfg) {
  if (! src_cfg.hasType()) { return; }
  // Check if buffer type matches
  if (Config::typeId<uint8_t>() != src_cfg.type()) {
    ConfigError err;
    err << "Can not configure AX25: Invalid type " << src_cfg.type()
        << ", expected " << Config::typeId<uint8_t>();
    throw err;
  }

  _bitstream = 0;
  _bitbuffer = 0;
  _state     = 0;
  _ptr       = _rxbuffer;

  // Allocate output buffer
  _buffer = Buffer<uint8_t>(512);

  LogMessage msg(LOG_DEBUG);
  msg << "Config AX.25 node.";
  Logger::get().log(msg);

  // propergate config
  this->setConfig(Config(Traits<uint8_t>::scalarId, 0, 512, 1));
}

void
AX25::process(const Buffer<uint8_t> &buffer, bool allow_overwrite)
{
  for (size_t i=0; i<buffer.size(); i++) {
    // Store bit in stream
    _bitstream = ((_bitstream << 1) | (buffer[i] & 0x01));

    // Check for sync byte
    if ((_bitstream & 0xff) == 0x7e) {
      if ((1==_state) && ((_ptr - _rxbuffer) > 2)) {
        *_ptr = 0;
        if (! check_crc_ccitt(_rxbuffer, _ptr-_rxbuffer)) {
          LogMessage msg(LOG_DEBUG);
          msg << "AX.25: Received invalid buffer: " << _rxbuffer;
          Logger::get().log(msg);
        } else {
          bool addrExt;
          std::string src, dst;
          int srcSSID, dstSSID;
          unpackCall(_rxbuffer, dst, dstSSID, addrExt);
          unpackCall(_rxbuffer+7, src, srcSSID, addrExt);
          std::cerr << "RX: " << src << "-" << srcSSID
                    << " > " << dst << "-" << dstSSID  << std::endl
                    << " " << _rxbuffer+14 << std::endl;
          memcpy(_buffer.ptr(), _rxbuffer, _ptr-_rxbuffer);
          this->send(_buffer.head(_ptr-_rxbuffer));
        }
      }
      // Receive data
      _state = 1;
      _ptr = _rxbuffer;
      _bitbuffer = 0x80;
      continue;
    }

    // If 7 ones are received in a row -> error, wait or sync byte
    if ((_bitstream & 0x7f) == 0x7f) { _state = 0; continue; }

    // If state == wait for sync byte -> receive next bit
    if (!_state) { continue; }

    /* stuffed bit */
    if ((_bitstream & 0x3f) == 0x3e) { continue; }

    // prepend bit to bitbuffer
    /// @todo Double check this!!!
    _bitbuffer |= ((_bitstream & 0x01) << 8);

    // If 8 bits have been received (stored in b8-b1 of _bitbuffer)
    if (_bitbuffer & 0x01) {
      // Check for buffer overrun
      if ((_ptr-_rxbuffer) >= 512) {
        Logger::get().log(LogMessage(LOG_DEBUG, "AX.25 packet too long."));
        // Wait for next sync byte
        _state = 0;
        continue;
      }

      // Store received byte and ...
      *_ptr++ = (_bitbuffer >> 1);
      // reset bit buffer
      _bitbuffer = 0x80;
      continue;
    }

    // Shift bitbuffer one to the left
    _bitbuffer >>= 1;
  }
}


void
AX25::unpackCall(const uint8_t *buffer, std::string &call, int &ssid, bool &addrExt) {
  size_t length = 0; call.resize(6);
  for (size_t i=0; i<6; i++) {
    call.at(i) = char(buffer[i]>>1);
    if (' ' != call.at(i)) { length++; }
  }
  call.resize(length);
  ssid = int( (buffer[6] & 0x1f) >> 1);
  addrExt = !bool(buffer[6] & 0x01);
}

