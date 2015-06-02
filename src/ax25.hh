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
class AX25: public Sink<uint8_t>, public Source
{
public:
  /** Constructor. */
  AX25();
  /** Destructor. */
  virtual ~AX25();
  /** Configures the node. */
  virtual void config(const Config &src_cfg);
  /** Processes the bit stream. */
  virtual void process(const Buffer<uint8_t> &buffer, bool allow_overwrite);

  /** Unpacks a AX.25 encoded call (address). */
  static void unpackCall(const uint8_t *buffer, std::string &call, int &ssid, bool &addrExt);

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

  /** Output buffer. */
  Buffer<uint8_t> _buffer;
};

}


#endif // __SDR_AX25_HH__
