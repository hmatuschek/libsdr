#ifndef __SDR_POSAG_HH__
#define __SDR_POSAG_HH__

#include "node.hh"

namespace sdr {

/** Implements a POCSAG decoder.
 * In conjecture with the @c FSKDetector or @c AFSDetector and the @c BitStream nodes, this node
 * can be used to receive and process POCSAG (pages) messages.
 *
 * The POCSAG protocol is defined as followig:
 *
 *  1) at least 576 bits of alternating value (1 0 1 0 ...)
 *  2) a 32-bit sync word (0x7CD215D8)
 *  3) 16 data words (each 32 bit)
 *
 * Unused data words are send as 0x7A89C197. Each dataword is either a address word (bit 31 = 0)
 * or message word (bit 31 = 1).
 *
 * Address word:
 *
 * +---+---+---+---+---+---+---+---+
 * | 0 |   Address (18bits)        |
 * +---+---+---+---+---+---+---+---+
 * |            ...                |
 * +---+---+---+---+---+---+---+---+
 * |    ...    | F1 F0 | ECC ...   |
 * +---+---+---+---+---+---+---+---+
 * |        ... (10 bits)      | P |
 * +---+---+---+---+---+---+---+---+
 *
 * Message word
 *
 * +---+---+---+---+---+---+---+---+
 * | 0 |   Data (20bits)           |
 * +---+---+---+---+---+---+---+---+
 * |            ...                |
 * +---+---+---+---+---+---+---+---+
 * |    ...            | ECC ...   |
 * +---+---+---+---+---+---+---+---+
 * |        ... (10 bits)      | P |
 * +---+---+---+---+---+---+---+---+
 *
 * @ingroup datanodes */
class POCSAG: public Sink<uint8_t>
{
public:
  class Message {
  public:
    Message();
    Message(uint32_t addr, uint8_t func);
    Message(const Message &other);

    Message &operator=(const Message &other);

    inline bool isEmpty() const { return _empty; }
    inline uint32_t address() const { return _address; }
    inline uint8_t function() const { return _function; }
    inline uint32_t bits() const { return _bits; }
    void addPayload(uint32_t word);

  protected:
    uint32_t             _address;
    uint8_t              _function;
    bool                 _empty;
    uint32_t             _bits;
    std::vector<uint8_t> _payload;
  };

protected:
  typedef enum {
    WAIT,
    RECEIVE,
    CHECK_CONTINUE
  } State;

public:
  POCSAG();

  void config(const Config &src_cfg);
  void process(const Buffer<uint8_t> &buffer, bool allow_overwrite);

  virtual void handleMessages();

protected:
  void _process_word(uint32_t word);
  void _reset_all_messages();
  void _reset_message(uint8_t slot);
  void _finish_all_messages();
  void _finish_message(uint8_t slot);

protected:
  State    _state;
  uint64_t _bits;
  uint8_t  _bitcount;
  uint8_t  _slot;

  Message _messages[8];
  std::list<Message> _queue;
};

}

#endif // POSAG_HH
