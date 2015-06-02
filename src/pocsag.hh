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
 *  3) 2x8 data words (each 32 bit)
 *  4) If data left to send -> continue with step 2
 *
 * Unused data words are send as 0x7A89C197. Each dataword is either a address word (bit 31 = 0)
 * or message word (bit 31 = 1).
 *
 * @ingroup datanodes */
class POCSAG: public Sink<uint8_t>
{
public:
  /** A posac message. */
  class Message {
  public:
    /** Empty constructor. */
    Message();
    /** Constructor from address and function. */
    Message(uint32_t addr, uint8_t func);
    /** Copy constructor. */
    Message(const Message &other);

    /** Assignment operator. */
    Message &operator=(const Message &other);

    /** Retruns @c true if the message is empty (has no address). */
    inline bool isEmpty() const { return _empty; }
    /** Returns the address of the message. */
    inline uint32_t address() const { return _address; }
    /** Returns the function of the message. */
    inline uint8_t function() const { return _function; }
    /** Returns the number of data bits. */
    inline uint32_t bits() const { return _bits; }

    /** Adds some payload from the given POGSAC word. */
    void addPayload(uint32_t word);

    int estimateText() const;
    int estimateNumeric() const;

    /** Decodes the message as a text message. */
    std::string asText() const;
    /** Decodes the message as a numeric message. */
    std::string asNumeric() const;
    /** Dumps the payload. */
    std::string asHex() const;

  protected:
    /** The address of the message. */
    uint32_t             _address;
    /** The function of the message. */
    uint8_t              _function;
    /** If @c true the message is empty. */
    bool                 _empty;
    /** The number of payload bits in the message. */
    uint32_t             _bits;
    /** The actual payload. */
    std::vector<uint8_t> _payload;
  };

protected:
  /** The possible states of the POGSAC receiver. */
  typedef enum {
    WAIT,           ///< Wait for a sync word.
    RECEIVE,        ///< Receive data.
    CHECK_CONTINUE  ///< Wait for the sync word for continuation.
  } State;

public:
  /** Constructor. */
  POCSAG();

  void config(const Config &src_cfg);
  void process(const Buffer<uint8_t> &buffer, bool allow_overwrite);

  /** Can be overwritten by any other implementation to process the received messages
   * stored in @c _queue. */
  virtual void handleMessages();

protected:
  /** Process a POGSAC word. */
  void _process_word(uint32_t word);
  /** Clear the message. */
  void _reset_message();
  /** Add the (non-empty) message to the queue. */
  void _finish_message();

protected:
  /** The current state. */
  State    _state;
  /** The last received bits. */
  uint64_t _bits;
  /** The number of received bits. */
  uint8_t  _bitcount;
  /** The current slot. */
  uint8_t  _slot;
  /** The current message. */
  Message _message;
  /** The completed messages. */
  std::list<Message> _queue;
};


/** A simple extention of the @c POCSAG node that prints the received messages to a
 * @c std::ostream.
 * @ingroup datanodes */
class POCSAGDump: public POCSAG
{
public:
  /** Constructor.
   * @param stream Specifies the stream, the received messages are serialized into. */
  POCSAGDump(std::ostream &stream);

  /** Dumps the received messages. */
  void handleMessages();

protected:
  /** The output stream. */
  std::ostream &_stream;
};


}

#endif // __SDR_POSAG_HH__
