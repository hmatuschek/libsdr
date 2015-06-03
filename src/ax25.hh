#ifndef __SDR_AX25_HH__
#define __SDR_AX25_HH__

#include "node.hh"

namespace sdr {

/** Decodes AX25 (PacketRadio) messages from a bit stream.
 *
 * In conjecture with the (A)FSK demodulator, the AX25 can be used to receive packet radio or APRS
 * messages. AX25 is usually transmitted as FSK in transition mode, means the bits aren't
 * encoded by mark & space tones but rather as a transition from mark to space or in reverse. Hence
 * the FSK node needs to be configured in transition mode.
 *
 * The node does not process the actual AX.25 packages, it only checks the frame check sequence and
 * forwards the AX.25 datagram to all connected sinks on success. The receiving node is responsible
 * for unpacking and handling the received datagram.
 * @ingroup datanodes */
class AX25: public Sink<uint8_t>
{
public:
  class Address
  {
  public:
    Address();
    Address(const std::string &call, size_t ssid);
    Address(const Address &other);

    Address &operator=(const Address &other);

    inline bool isEmpty() const { return 0 == _call.size(); }
    inline const std::string &call() const { return _call; }
    inline size_t ssid() const { return _ssid; }

  protected:
    std::string _call;
    size_t _ssid;
  };

  class Message
  {
  public:
    Message();
    Message(uint8_t *buffer, size_t length);
    Message(const Message &other);

    Message &operator=(const Message &other);

    inline const Address &from() const { return _from; }
    inline const Address &to() const { return _to; }
    inline const std::vector<Address> &via() const { return _via; }

    inline const std::string &payload() const { return _payload; }

  protected:
    Address _from;
    Address _to;
    std::vector<Address> _via;
    std::string _payload;
  };

public:
  /** Constructor. */
  AX25();
  /** Destructor. */
  virtual ~AX25();

  /** Configures the node. */
  virtual void config(const Config &src_cfg);
  /** Processes the bit stream. */
  virtual void process(const Buffer<uint8_t> &buffer, bool allow_overwrite);

  virtual void handleAX25Message(const Message &message);

protected:
  /** The last bits. */
  uint32_t _bitstream;
  /** A buffer of received bits. */
  uint32_t _bitbuffer;
  /** The current state. */
  uint32_t _state;

  /** Message buffer. */
  uint8_t _rxbuffer[512];
  /** Insert-pointer to the buffer. */
  uint8_t *_ptr;
};


/** Prints received AX25 messages to the specified stream. */
class AX25Dump: public AX25
{
public:
  /** Constructor.
   * @param stream The output stream. */
  AX25Dump(std::ostream &stream);

  /** Implements AX25 interface. */
  void handleAX25Message(const Message &message);

protected:
  /** The output stream. */
  std::ostream &_stream;
};


/** Serialization of AX25 address. */
std::ostream& operator<<(std::ostream &stream, const sdr::AX25::Address &addr);
/** Serialization of AX25 message. */
std::ostream& operator<<(std::ostream &stream, const sdr::AX25::Message &msg);

} // namespace sdr


#endif // __SDR_AX25_HH__
