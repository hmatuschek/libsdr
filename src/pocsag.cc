#include "pocsag.hh"
#include "bch31_21.hh"

using namespace sdr;

inline bool is_address(uint32_t word) {
  return (0 == (0x80000000 & word));
}


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
        _reset_all_messages();
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
          _finish_all_messages(); _state    = WAIT;
          this->handleMessages();
        }
      }
    }
  }
}


void
POCSAG::_process_word(uint32_t word)
{
  // Check if slot is skipped
  if (0x7A89C197 == word) { // Skip slot
    _finish_message(_slot); return;
  }

  // Check if word is an address word
  if (is_address(word)) {
    _finish_message(_slot);
    // Assemble address
    uint32_t addr = (((word>>13) & 0x03ffff)<<3) | (_slot & 0x03);
    uint8_t  func = ((word>>11) & 0x03);
    _messages[_slot] = Message(addr, func);
  } else {
    if (_messages[_slot].isEmpty()) {
      std::cerr << "Oops: Payload w/o address in slot " << int(_slot)
                << " word: " << std::hex << word << std::dec << std::endl;
    }
    _messages[_slot].addPayload(word);
  }
}


void
POCSAG::_reset_message(uint8_t slot) {
  _messages[slot] = Message();
}

void
POCSAG::_reset_all_messages() {
  for (uint8_t i=0; i<8; i++) {
    _reset_message(i);
  }
}

void
POCSAG::_finish_message(uint8_t slot) {
  if (_messages[slot].isEmpty()) { return; }
  _queue.push_back(_messages[slot]);
  _reset_message(slot);
}

void
POCSAG::_finish_all_messages() {
  for (uint8_t i=0; i<8; i++) {
    _finish_message(i);
  }
}

void
POCSAG::handleMessages() {
  // You may re-implement this virutal method to process the queued messages in _queue.
  while (_queue.size()) {
    Message msg = _queue.back(); _queue.pop_back();
    std::cerr << "POCSAG: @" << msg.address()
              << ", F=" << int(msg.function())
              << ", bits=" << msg.bits() << std::endl;
  }
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
  // Add 20 LSB bits from data to payload vector
  uint32_t mask = 0x40000000;
  for (size_t i=0; i<20; i++) {
    // on new byte
    if (0 == (_bits % 8)) { _payload.push_back(0x00); }
    // add bit to last byte of payload
    _payload.back() = ((_payload.back()<<1)|(word&mask));
    // Increment bit counter and update mask
    _bits++; mask = (mask>>1);
  }
}

