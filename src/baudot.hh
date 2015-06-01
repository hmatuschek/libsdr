#ifndef __SDR_BAUDOT_HH__
#define __SDR_BAUDOT_HH__

#include "node.hh"
#include <map>

namespace sdr {

/** Implements a Baudot decoder. Inconjecture with the (A)FSK demodulator, it enables the
 * reception of radio teletype (RTTY) messages.
 *
 * Please note that a baudot encoded char is usually transmitted in a frame with one start bit and
 * 1, 1.5 or 2 stop bits. Hence this node expects to receive two bits for one decoded bit in order
 * to detect the 1.5 stop bits reliably.
 *
 * I.e. to receive a 45.45 baud RTTY signal, the (A)FSK demodulator need to be configured for
 * 90.90 baud (= 2*45.45 baud).
 * @ingroup datanodes */
class Baudot: public Sink<uint8_t>, public Source
{
public:
  /** Specifies the current code-tables. */
  typedef enum {
    LETTERS,   ///< Letters.
    FIGURES    ///< Numbers, symbols etc.
  } Mode;

  /** Specifies the number of stop bits. */
  typedef enum {
    STOP1,   ///< 1 stop bit.
    STOP15,  ///< 1.5 stop bits.
    STOP2    ///< 2 stop bits.
  } StopBits;

public:
  /** Constructor. */
  Baudot(StopBits stopBits = STOP15);

  /** Configures the node. */
  virtual void config(const Config &src_cfg);
  /** Processes the bit-stream. */
  virtual void process(const Buffer<uint8_t> &buffer, bool allow_overwrite);

protected:
  /** Code table for letters. */
  static char _letter[32];
  /** Code table for symbols or figure (i.e. numbers). */
  static char _figure[32];
  /** The last bits received. */
  uint16_t _bitstream;
  /** The number of bits received. */
  size_t   _bitcount;

  /** The currently selected table. */
  Mode     _mode;

  /** Specifies the number of half bits per symbol. */
  size_t   _bitsPerSymbol;
  /** Specifies the frame pattern. */
  uint16_t _pattern;
  /** Specifies the frame mask. */
  uint16_t _mask;
  /** Number of half bits forming the stop bit. */
  uint16_t _stopHBits;

  /** The output buffer. */
  Buffer<uint8_t> _buffer;
};

}

#endif // BAUDOT_HH
