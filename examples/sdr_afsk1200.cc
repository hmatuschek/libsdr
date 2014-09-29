#include "wavfile.hh"
#include "autocast.hh"
#include "interpolate.hh"

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


class AFSK: public Sink<int16_t>, public Source {
public:
  AFSK(double baud=1200.0, double Fmark=1200.0, double Fspace=2200.0)
    : Sink<int16_t>(), Source(), _baud(baud), _Fmark(Fmark), _Fspace(Fspace)
  {
    // pass...
  }

  virtual ~AFSK() {
    // pass...
  }

  virtual void config(const Config &src_cfg) {
    // Check if config is complete
    if (!src_cfg.hasType() || !src_cfg.hasSampleRate()) { return; }

    // Check if buffer type matches
    if (Config::typeId<int16_t>() != src_cfg.type()) {
      ConfigError err;
      err << "Can not configure AFSK1200: Invalid type " << src_cfg.type()
          << ", expected " << Config::typeId<int16_t>();
      throw err;
    }

    // The input sample rate
    _sampleRate = src_cfg.sampleRate();

    // Samples per bit
    _corrLen = int(_sampleRate/_baud);
    // Compute symbol rate:
    _symbolRate = std::min(10*_baud, _baud*_corrLen);

    // Samples per symbol (fractional):
    _mu = 0.0; _muIncr = _sampleRate/_symbolRate;
    // Delayline for interpolating sub-sampler
    _dl = Buffer<float>(2*8);
    for (size_t i=0; i<(2*8); i++) { _dl[i] = 0; }
    _dl_idx = 0;

    // Assemble phase LUT:
    _markLUT  = Buffer< std::complex<float> >(_corrLen);
    _spaceLUT = Buffer< std::complex<float> >(_corrLen);

    // Allocate ring-buffers for mark and space symbols
    _markHist = Buffer< std::complex<float> >(_corrLen);
    _spaceHist = Buffer< std::complex<float> >(_corrLen);

    // Initialize LUTs and ring-buffers
    double phiMark=0, phiSpace=0;
    for (size_t i=0; i<_corrLen; i++) {
      _markLUT[i] = std::exp(std::complex<float>(0.0, phiMark));
      _spaceLUT[i] = std::exp(std::complex<float>(0.0, phiSpace));
      phiMark  += (2.*M_PI*_Fmark)/_sampleRate;
      phiSpace += (2.*M_PI*_Fspace)/_sampleRate;
      _markHist[i] = 0; _spaceHist[i] = 0;
    }
    _lutIdx = 0;

    // Get phase increment per symbol
    _phase = 0; _omega = _baud/_symbolRate;
    _omegaMin = _omega - 0.01*_omega;
    _omegaMax = _omega + 0.01*_omega;
    _gainOmega = 0.01;

    // Allocate output buffer:
    _buffer = Buffer<uint8_t>(src_cfg.bufferSize()/_corrLen + 1);

    LogMessage msg(LOG_DEBUG);
    msg << "Config AFSK1200 node: " << std::endl
        << " input sample rate: " << _sampleRate << "Hz" << std::endl
        << " samples per symbol: " << _muIncr << std::endl
        << " symbols per bit: " << _corrLen << std::endl
        << " symbol rate: " << _symbolRate << "Hz" << std::endl
        << " Phase incr/symbol: " << float(_omega);

    Logger::get().log(msg);

    this->setConfig(Config(Traits<uint8_t>::scalarId, _baud, _buffer.size(), 1));
  }


  virtual void process(const Buffer<int16_t> &buffer, bool allow_overwrite) {
    size_t i=0, o=0;
    while (i<buffer.size()) {
      // Update sub-sampler
      while ((_mu>1) && (i<buffer.size())) {
        _markHist[_lutIdx] = float(buffer[i])*_markLUT[_lutIdx];
        _spaceHist[_lutIdx] = float(buffer[i])*_spaceLUT[_lutIdx];
        // Modulo LUT length
        _lutIdx++; if (_lutIdx==_corrLen) { _lutIdx=0; }
        float symbol = _getSymbol();
        // Put symbol into delay line
        _dl[_dl_idx] = symbol; _dl[_dl_idx+8] = symbol;
        _dl_idx = (_dl_idx+1)%8; _mu -= 1; i++;
      }


      if (i<buffer.size()) {
        // Get interpolated symbol
        float sample = interpolate(_dl.sub(_dl_idx, 8), _mu); _mu += _muIncr;
        // Get symbol
        _symbols <<= 1; _symbols |= (sample>0);
        // Advance phase
        _phase += _omega;

        // Sample bit
        if (_phase >= 1) {
          // Modulo "2 pi"
          _phase = fmodf(_phase, 1.0);
          // Store bit
          _lastBits <<= 1; _lastBits |= (_symbols & 1);
          // Put decoded bit in output buffer
          // transition -> 0; no transition -> 1
          _buffer[o++] = ((_lastBits ^ (_lastBits >> 1) ^ 1) & 1);
        }

        // If transition
        if ((_symbols ^ (_symbols >> 1)) & 1) {
          // Phase correction
          /*std::cerr << "Transition at phi=" << _phase << std::endl
                    << "  update omega from " << _omega << " to "; */
          if (_phase < 0.5) { _omega -= _gainOmega*(_phase); }
          else { _omega += _gainOmega*(1-_phase); }
          // Limit omega
          _omega = std::min(_omegaMax, std::max(_omegaMin, _omega));
          //std::cerr << _omega << std::endl;
        }
      }
    }
    this->send(_buffer.head(o));
  }


protected:
  inline double _getSymbol() {
    std::complex<double> markSum(0), spaceSum(0);
    for (size_t i=0; i<_corrLen; i++) {
      markSum += _markHist[i];
      spaceSum += _spaceHist[i];
    }
    double f = markSum.real()*markSum.real() +
        markSum.imag()*markSum.imag() -
        spaceSum.real()*spaceSum.real() -
        spaceSum.imag()*spaceSum.imag();
    return f;
  }


protected:
  float _sampleRate, _symbolRate;
  float _baud;
  float _Fmark, _Fspace;

  uint32_t _corrLen;
  uint32_t _lutIdx;
  Buffer< std::complex<float> > _markLUT;
  Buffer< std::complex<float> > _spaceLUT;

  float _mu, _muIncr;
  Buffer< float > _dl;
  size_t _dl_idx;

  Buffer< std::complex<float> > _markHist;
  Buffer< std::complex<float> > _spaceHist;

  uint32_t _symbols;
  uint32_t _lastBits;
  float _phase;
  float _omega, _omegaMin, _omegaMax;
  float _gainOmega;

  static const uint32_t _phasePeriod = 0x10000u;
  static const uint32_t _phaseMask   = 0x0ffffu;

  /** Output buffer. */
  Buffer<uint8_t> _buffer;
};


class AX25: public Sink<uint8_t>, public Source
{
public:
  AX25()
    : Sink<uint8_t>(), Source()
  {
    // pass...
  }

  virtual ~AX25() {
    // pass...
  }

  virtual void config(const Config &src_cfg) {
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

    // propergate config
    this->setConfig(Config(Traits<uint8_t>::scalarId, 0, 512, 1));
  }

  virtual void process(const Buffer<uint8_t> &buffer, bool allow_overwrite) {
    for (size_t i=0; i<buffer.size(); i++) {
      // Store bit in stream
      _bitstream <<= 1; _bitstream |= !!buffer[i];

      // Check for sync byte
      if ((_bitstream & 0xff) == 0x7e) {
        if (_state && ((_ptr - _rxbuffer) > 2)) {
          *_ptr = 0;
          if (! check_crc_ccitt(_rxbuffer, _ptr-_rxbuffer)) {
            std::cerr << "Got invalid buffer: " << _rxbuffer << std::endl;
          } else {
            std::cerr << "GOT: " << _rxbuffer << std::endl;
            memcpy(_buffer.ptr(), _rxbuffer, _ptr-_rxbuffer);
            this->send(_buffer.head(_ptr-_rxbuffer));
          }

        }
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
      _bitbuffer |= ((_bitstream & 1) << 8);

      // If 8 bits have been received (stored in b8-b1 of _bitbuffer)
      if (_bitbuffer & 1) {
        // Check for buffer overrun
        if ((_ptr-_rxbuffer) >= 512) {
          Logger::get().log(LogMessage(LOG_ERROR, "AX.25 packet too long."));
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

protected:
  uint32_t _bitstream;
  uint32_t _bitbuffer;
  uint32_t _state;

  uint8_t _rxbuffer[512];
  uint8_t *_ptr;

  Buffer<uint8_t> _buffer;
};


int main(int argc, char *argv[]) {
  if (2 > argc) { std::cout << "USAGE: sdr_afsk1200 FILENAME" << std::endl; return -1; }

  sdr::Logger::get().addHandler(
        new sdr::StreamLogHandler(std::cerr, sdr::LOG_DEBUG));

  WavSource src(argv[1], 1024);
  if (! src.isOpen()) { std::cout << "Can not open file " << argv[1] << std::endl; return -1; }

  AutoCast< int16_t > cast;
  AFSK demod;
  AX25     decode;

  src.connect(&cast);
  cast.connect(&demod);
  demod.connect(&decode);

  Queue::get().addIdle(&src, &WavSource::next);

  Queue::get().start();
  Queue::get().wait();

  return 0;
}
