#include "afsk.hh"
#include "logger.hh"
#include "traits.hh"
#include "interpolate.hh"

using namespace sdr;


AFSK::AFSK(double baud, double Fmark, double Fspace, Mode mode)
  : Sink<int16_t>(), Source(), _baud(baud), _Fmark(Fmark), _Fspace(Fspace), _mode(mode)
{
  // pass...
}

AFSK::~AFSK() {
  // pass...
}

void
AFSK::config(const Config &src_cfg)
{
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
  _symbolRate = std::max(_baud, _baud*_corrLen);

  // Samples per symbol (fractional):
  _muIncr = _sampleRate/_symbolRate;
  _mu     = _muIncr;
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
  _symbols   = Buffer<int16_t>(_corrLen);

  // Initialize LUTs and ring-buffers
  double phiMark=0, phiSpace=0;
  for (size_t i=0; i<_corrLen; i++) {
    _markLUT[i] = std::exp(std::complex<float>(0.0, phiMark));
    _spaceLUT[i] = std::exp(std::complex<float>(0.0, phiSpace));
    phiMark  += (2.*M_PI*_Fmark)/_sampleRate;
    phiSpace += (2.*M_PI*_Fspace)/_sampleRate;
    // Apply Window functions
    //_markLUT[i] *= (0.42 - 0.5*cos((2*M_PI*i)/_corrLen) + 0.08*cos((4*M_PI*i)/_corrLen));
    //_spaceLUT[i] *= (0.42 - 0.5*cos((2*M_PI*i)/_corrLen) + 0.08*cos((4*M_PI*i)/_corrLen));
    _markHist[i] = 0; _spaceHist[i] = 0; _symbols[i] = 0;
  }
  // Ring buffer indices
  _lutIdx = 0; _symbolIdx = 0;

  // Get phase increment per symbol
  _phase = 0; _omega = _baud/_symbolRate;
  // PLL limits +/- 10% around _omega
  _omegaMin = _omega - 0.005*_omega;
  _omegaMax = _omega + 0.005*_omega;
  // PLL gain
  _gainOmega = 0.0005;

  _symSum  = 0;
  _lastSymSum = 0;

  // Allocate output buffer:
  _buffer = Buffer<uint8_t>(src_cfg.bufferSize()/_corrLen + 1);

  LogMessage msg(LOG_DEBUG);
  msg << "Config AFSK node: " << std::endl
      << " input sample rate: " << _sampleRate << " Hz" << std::endl
      << " samples per symbol: " << _muIncr << std::endl
      << " symbols per bit: " << _corrLen << std::endl
      << " symbol rate: " << _symbolRate << " Hz" << std::endl
      << " bit rate: " << _symbolRate/_corrLen << " baud" << std::endl
      << " phase incr/symbol: " << float(_omega) << std::endl
      << " bit mode: " << ((TRANSITION == _mode) ? "transition" : "normal");
  Logger::get().log(msg);

  // Forward config.
  this->setConfig(Config(Traits<uint8_t>::scalarId, _baud, _buffer.size(), 1));
}


void
AFSK::process(const Buffer<int16_t> &buffer, bool allow_overwrite)
{
  size_t i=0, o=0;
  while (i<buffer.size()) {
    // Update sub-sampler
    while ((_mu>=1) && (i<buffer.size())) {
      _markHist[_lutIdx] = float(buffer[i])*_markLUT[_lutIdx];
      _spaceHist[_lutIdx] = float(buffer[i])*_spaceLUT[_lutIdx];
      // inc _lutIdx, modulo LUT length
      _lutIdx++; if (_lutIdx==_corrLen) { _lutIdx=0; }
      // Get symbol from FIR filter results
      float symbol = _getSymbol();
      // Put symbol into delay line
      _dl[_dl_idx] = symbol; _dl[_dl_idx+8] = symbol;
      _dl_idx = (_dl_idx+1)%8; _mu -= 1; i++;
    }

    if (_mu >= 1) { continue; }

    // Get interpolated symbol
    float symbol = interpolate(_dl.sub(_dl_idx, 8), _mu); _mu += _muIncr;
    // store symbol
    _lastSymSum = _symSum;
    _symSum -= _symbols[_symbolIdx];
    _symbols[_symbolIdx] = ( (symbol>=0) ? 1 : -1 );
    _symSum += _symbols[_symbolIdx];
    _symbolIdx = ((_symbolIdx+1) % _corrLen);
    // Advance phase
    _phase += _omega;

    // Sample bit
    if (_phase >= 1) {
      // Modulo "2 pi", phase is defined on the interval [0,1)
      while (_phase>=1) { _phase -= 1; }
      // Estimate bit by majority vote on all symbols
      _lastBits = ((_lastBits<<1) | (_symSum>0));
      // Put decoded bit in output buffer
      if (TRANSITION == _mode) {
        // transition -> 0; no transition -> 1
        _buffer[o++] = ((_lastBits ^ (_lastBits >> 1) ^ 1) & 1);
      } else {
        // mark -> 1, space -> 0
        _buffer[o++] = _lastBits & 1;
      }
    }

    // If there was a symbol transition
    if (((_lastSymSum < 0) && (_symSum>=0)) || ((_lastSymSum >= 0) && (_symSum<0))) {
      // Phase correction
      /**std::cerr << "Transition at phi=" << _phase << std::endl
                  << "  update omega from " << _omega << " to "; */
      // transition at [-pi,0] -> increase omega
      if (_phase < 0.5) { _omega += _gainOmega*(0.5-_phase); }
      // transition at [0,pi]  -> decrease omega
      else { _omega -= _gainOmega*(_phase-0.5); }
      // Limit omega
      _omega = std::min(_omegaMax, std::max(_omegaMin, _omega));
      //_phase += _gainOmega*(_phase-0.5);
      /* std::cerr << _omega << std::endl; */
    }
  }

  if (0 < o) { this->send(_buffer.head(o)); }
}

