#include "pocsag.hh"
#include "bch31_21.hh"
#include "logger.hh"

using namespace sdr;

inline bool is_address(uint32_t word) {
  return (0 == (0x80000000 & word));
}


/* ********************************************************************************************* *
 * Implementation of POCSAG
 * ********************************************************************************************* */
POCSAG::POCSAG()
  : Sink<uint8_t>()
{
  // pass...
}

void
POCSAG::config(const Config &src_cfg) {
  if (! src_cfg.hasType()) { return; }
  // Check if buffer type matches
  if (Config::typeId<uint8_t>() != src_cfg.type()) {
    ConfigError err;
    err << "Can not configure POCSAG: Invalid type " << src_cfg.type()
        << ", expected " << Config::typeId<uint8_t>();
    throw err;
  }

  LogMessage msg(LOG_DEBUG);
  msg << "Config POCSAG node.";
  Logger::get().log(msg);

  _state = WAIT;
  _bits  = 0;
}

void
POCSAG::process(const Buffer<uint8_t> &buffer, bool allow_overwrite)
{
  for (size_t i=0; i<buffer.size(); i++)
  {
    // put bit into shift register
    _bits = ( (_bits<<1) | (buffer[i] & 0x01) );

    // Dispatch by state
    if (WAIT == _state) {
      // Wait for the sync word to appear
      uint32_t word = (_bits & 0xffffffff);
      if ( (0 == pocsag_repair(word)) && (0x7cd215d8 == word)) {
        // init messages
        _reset_message();
        _state = RECEIVE; _bitcount = 0; _slot = 0;
      }
    } else if (RECEIVE == _state) {
      // Receive 64 bit (2 words)
      _bitcount++;
      if (64 == _bitcount) {
        _bitcount=0;

        // get and check 1st word bits
        uint32_t word = ( (_bits>>32) & 0xffffffff );
        if (0 == pocsag_repair(word)) { _process_word(word); }

        // get and check 2nd word bits
        word = ( _bits & 0xffffffff );
        if (0 == pocsag_repair(word)) { _process_word(word); }

        // Advance slot counter
        _slot++;
        if (8 == _slot) {
          // If all slots (8) has been processed -> wait for continuation
          _state = CHECK_CONTINUE;
        }
      }
    } else if (CHECK_CONTINUE == _state) {
      // Wait for an immediate sync word
      _bitcount++;
      if (32 == _bitcount) {
        uint32_t word = (_bits&0xffffffff);
        if ( (0 == pocsag_repair(word)) && (0x7cd215d8 == word)) {
          // If a sync word has been received -> continue with reception of slot 0
          _state = RECEIVE; _slot = 0; _bitcount = 0;
        } else {
          // Otherwise -> end of transmission, wait for next sync
          _finish_message(); _state = WAIT;
          // Process received messages
          this->handleMessages();
        }
      }
    }
  }
}


void
POCSAG::_process_word(uint32_t word)
{
  /*std::cerr << "POCSAG: RX " << std::hex << word
            << std::dec << " @ " << int(_slot) << std::endl; */


  if (0x7A89C197 == word) {
    // Skip
    _finish_message();
  } else if (is_address(word)) {
    // If address word
    _finish_message();
    // Assemble address
    uint32_t addr = ((((word>>13) & 0x03ffff)<<3) + _slot );
    uint8_t  func = ((word>>11) & 0x03);
    // init new message
    _message = Message(addr, func);
  } else {
    // on data word
    if (_message.isEmpty()) {
      LogMessage msg(LOG_DEBUG);
      msg << "POCSAG: Payload w/o address in slot " << int(_slot)
          << " word: " << std::hex << word;
      Logger::get().log(msg);
    } else {
      _message.addPayload(word);
    }
  }
}


void
POCSAG::_reset_message() {
  _message = Message();
}

void
POCSAG::_finish_message() {
  if (_message.isEmpty()) { return; }
  _queue.push_back(_message);
  _reset_message();
}

void
POCSAG::handleMessages() {
  // pass...
}


/* ********************************************************************************************* *
 * Implementation of POCSAGDump
 * ********************************************************************************************* */
POCSAGDump::POCSAGDump(std::ostream &stream)
  : POCSAG(), _stream(stream)
{
  // pass...
}

void
POCSAGDump::handleMessages() {
  // You may re-implement this virutal method to process the queued messages in _queue.
  while (_queue.size()) {
    Message msg = _queue.back(); _queue.pop_back();
    std::cerr << "POCSAG: @" << msg.address()
              << ", F=" << int(msg.function())
              << ", bits=" << msg.bits() << std::endl;
    if (msg.estimateText() >= msg.estimateNumeric()) {
      std::cerr << " txt: " << msg.asText() << "" << std::endl;
    } else {
      std::cerr << " num: " << msg.asNumeric() << std::endl;
    }
    //std::cerr << " hex: " << msg.asHex() << std::endl;
  }
}


/* ********************************************************************************************* *
 * Implementation of POCSAG::Message
 * ********************************************************************************************* */
std::string
ascii2text(uint8_t byte) {
  switch ( byte ) {
  case  0: return "<NUL>";
  case  1: return "<SOH>";
  case  2: return "<STX>";
  case  3: return "<ETX>";
  case  4: return "<EOT>";
  case  5: return "<ENQ>";
  case  6: return "<ACK>";
  case  7: return "<BEL>";
  case  8: return "<BS>";
  case  9: return "<HT>";
  case 10: return "<LF>";
  case 11: return "<VT>";
  case 12: return "<FF>";
  case 13: return "<CR>";
  case 14: return "<SO>";
  case 15: return "<SI>";
  case 16: return "<DLE>";
  case 17: return "<DC1>";
  case 18: return "<DC2>";
  case 19: return "<DC3>";
  case 20: return "<DC4>";
  case 21: return "<NAK>";
  case 22: return "<SYN>";
  case 23: return "<ETB>";
  case 24: return "<CAN>";
  case 25: return "<EM>";
  case 26: return "<SUB>";
  case 27: return "<ESC>";
  case 28: return "<FS>";
  case 29: return "<GS>";
  case 30: return "<RS>";
  case 31: return "<US>";
  default: break;
  }
  std::string txt; txt.resize(1, char(byte));
  return txt;
}

char
bcd2text(uint8_t bcd) {
  static const char *conv_table = "084 2.6]195-3U7[";
  return conv_table[bcd&0xf];
}

int
pocsag_text_weight(char c) {
  if (c < 32 || c == 127) {
    return -5; // Non printable characters are uncommon
  }
  if ( (c > 32 && c < 48)
       || (c > 57 && c < 65)
       || (c > 90 && c < 97)
       || (c > 122 && c < 127) ) {
    return -2; // Penalize special characters
  }
  return 1;
}

int
pocsag_numeric_weight(char cp, size_t pos) {
  if(cp == 'U')
    return -10;
  if(cp == '[' || cp == ']')
    return -5;
  if(cp == ' ' || cp == '.' || cp == '-')
    return -2;
  if(pos < 10) // Penalize long messages
    return 5;
  return 0;
}


POCSAG::Message::Message()
  : _address(0), _function(0), _empty(true), _bits(0)
{
  // pass...
}

POCSAG::Message::Message(uint32_t addr, uint8_t func)
  : _address(addr), _function(func), _empty(false), _bits(0)
{
  // pass...
}

POCSAG::Message::Message(const Message &other)
  : _address(other._address), _function(other._function), _empty(other._empty),
    _bits(other._bits), _payload(other._payload)
{
  // pass...
}

POCSAG::Message &
POCSAG::Message::operator =(const Message &other) {
  _address = other._address;
  _function = other._function;
  _empty = other._empty;
  _bits = other._bits;
  _payload = other._payload;
  return *this;
}

void
POCSAG::Message::addPayload(uint32_t word) {
  // Add data bits from data to payload vector
  uint32_t mask = 0x40000000;
  for (int i=19; i>=0; i--) {
    // on new byte -> add empty byte to payload
    if (0 == (_bits % 8)) { _payload.push_back(0x00); }
    // add bit to last byte of payload
    _payload.back() = ((_payload.back()<<1) | ((word & mask)>>(i+11)));
    // Increment bit counter and update mask
    _bits++; mask = (mask>>1);
  }
}

std::string
POCSAG::Message::asText() const
{
  uint8_t byte = 0;
  // Decode message
  std::stringstream buffer;
  for (size_t i=0; i<_bits; i++) {
    size_t byteIdx = i/8;
    size_t bitIdx  = (7-(i%8));
    // add bit to byte (reverse order)
    byte = ((byte>>1) | (((_payload[byteIdx]>>bitIdx) & 0x01) << 6));
    if (6 == (i%7)) {
      buffer << ascii2text(byte&0x7f);
    }
    //std::cerr << "byte " << byteIdx << " bit " << bitIdx << " ascii bit " << (i%7) << std::endl;
  }
  return buffer.str();
}


std::string
POCSAG::Message::asNumeric() const
{
  // Get number of complete half-bytes in payload
  size_t N = _bits/4;
  // Decode message
  std::stringstream buffer;
  for (size_t i=0; i<(N/2); i++) {
    buffer << bcd2text((_payload[i]>>4) & 0xf);
    buffer << bcd2text((_payload[i]>>0) & 0xf);
  }
  if (N%2) {
    buffer << bcd2text((_payload[N/2]>>0) & 0xf);
  }
  return buffer.str();
}

std::string
POCSAG::Message::asHex() const {
  std::stringstream buffer;
  buffer << std::hex;
  for (size_t i=0; i<_payload.size(); i++) {
    buffer << int(_payload[i]);
  }
  return buffer.str();
}

int
POCSAG::Message::estimateText() const {
  int weight = 0;
  uint8_t byte = 0;
  for (size_t i=0; i<_bits; i++) {
    size_t byteIdx = i/8;
    size_t bitIdx  = (7-i%8);
    // add bit to byte
    byte = ((byte>>1) | (((_payload[byteIdx]>>bitIdx) & 0x01) << 6));
    if (6 == i%7) {
      weight += pocsag_text_weight(byte&0x7f);
    }
  }
  return weight;
}

int
POCSAG::Message::estimateNumeric() const {
  int weight = 0;
  // Get number of complete half-bytes in payload
  size_t N = _bits/4;
  for (size_t i=0; i<(N/2); i++) {
    weight += pocsag_numeric_weight(bcd2text((_payload[i]>>4) & 0xf), i);
    weight += pocsag_numeric_weight(bcd2text((_payload[i]>>0) & 0xf), i);
  }
  if (N%2) {
    weight += pocsag_numeric_weight(bcd2text((_payload[N/2]>>0) & 0xf), N/2);
  }
  return weight;
}
