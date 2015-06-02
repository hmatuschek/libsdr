#include "afsk.hh"
#include "logger.hh"
#include "traits.hh"
#include "interpolate.hh"

using namespace sdr;


FSKDetector::FSKDetector(float baud, float Fmark, float Fspace)
  : Sink<int16_t>(), Source(), _baud(baud), _corrLen(0), _Fmark(Fmark), _Fspace(Fspace)
{
  // pass...
}

void
FSKDetector::config(const Config &src_cfg)
{
  // Check if config is complete
  if (!src_cfg.hasType() || !src_cfg.hasSampleRate()) { return; }

  // Check if buffer type matches
  if (Config::typeId<int16_t>() != src_cfg.type()) {
    ConfigError err;
    err << "Can not configure FSKBase: Invalid type " << src_cfg.type()
        << ", expected " << Config::typeId<int16_t>();
    throw err;
  }

  _corrLen   = int(src_cfg.sampleRate()/_baud);
  _markLUT   = Buffer< std::complex<float> >(_corrLen);
  _spaceLUT  = Buffer< std::complex<float> >(_corrLen);
  _markHist  = Buffer< std::complex<float> >(_corrLen);
  _spaceHist = Buffer< std::complex<float> >(_corrLen);

  // Initialize LUTs and ring-buffers
  double phiMark=0, phiSpace=0;
  for (size_t i=0; i<_corrLen; i++) {
    _markLUT[i] = std::exp(std::complex<float>(0.0, phiMark));
    _spaceLUT[i] = std::exp(std::complex<float>(0.0, phiSpace));
    phiMark  += (2.*M_PI*_Fmark)/src_cfg.sampleRate();
    phiSpace += (2.*M_PI*_Fspace)/src_cfg.sampleRate();
    // Apply Window functions
    //_markLUT[i] *= (0.42 - 0.5*cos((2*M_PI*i)/_corrLen) + 0.08*cos((4*M_PI*i)/_corrLen));
    //_spaceLUT[i] *= (0.42 - 0.5*cos((2*M_PI*i)/_corrLen) + 0.08*cos((4*M_PI*i)/_corrLen));
    _markHist[i] = 0; _spaceHist[i] = 0;
  }
  // Ring buffer index
  _lutIdx = 0;

  // Allocate output buffer
  _buffer = Buffer<int8_t>(src_cfg.bufferSize());

  LogMessage msg(LOG_DEBUG);
  msg << "Config FSKDetector node: " << std::endl
      << " sample/symbol rate: " << src_cfg.sampleRate() << " Hz" << std::endl
      << " target baud rate: " << _baud << std::endl
      << " approx. samples per bit: " << _corrLen;
  Logger::get().log(msg);

  // Forward config.
  this->setConfig(Config(Traits<uint8_t>::scalarId, src_cfg.sampleRate(), src_cfg.bufferSize(), 1));
}


uint8_t
FSKDetector::_process(int16_t sample) {
  _markHist[_lutIdx] = float(sample)*_markLUT[_lutIdx];
  _spaceHist[_lutIdx] = float(sample)*_spaceLUT[_lutIdx];
  // inc _lutIdx, modulo LUT length
  _lutIdx++; if (_lutIdx==_corrLen) { _lutIdx=0; }

  std::complex<float> markSum(0), spaceSum(0);
  for (size_t i=0; i<_corrLen; i++) {
    markSum += _markHist[i];
    spaceSum += _spaceHist[i];
  }

  float f = markSum.real()*markSum.real() +
      markSum.imag()*markSum.imag() -
      spaceSum.real()*spaceSum.real() -
      spaceSum.imag()*spaceSum.imag();

  return (f>0);
}

void
FSKDetector::process(const Buffer<int16_t> &buffer, bool allow_overwrite) {
  for (size_t i=0; i<buffer.size(); i++) {
    _buffer[i] = _process(buffer[i]);
  }
  this->send(_buffer.head(buffer.size()), false);
}


BitStream::BitStream(float baud, Mode mode)
  : Sink<uint8_t>(), Source(), _baud(baud), _mode(mode), _corrLen(0)
{
  // pass...
}

void
BitStream::config(const Config &src_cfg) {
  // Check if config is complete
  if (!src_cfg.hasType() || !src_cfg.hasSampleRate()) { return; }

  // Check if buffer type matches
  if (Config::typeId<uint8_t>() != src_cfg.type()) {
    ConfigError err;
    err << "Can not configure FSKBitStreamBase: Invalid type " << src_cfg.type()
        << ", expected " << Config::typeId<int16_t>();
    throw err;
  }

  // # of symbols for each bit
  _corrLen = int(src_cfg.sampleRate()/_baud);

  // Config PLL for bit detection
  _phase    = 0;
  // exact bits per sample (<< 1)
  _omega   = _baud/src_cfg.sampleRate();
  // PLL limits +/- 10% around _omega
  _omegaMin = _omega - 0.005*_omega;
  _omegaMax = _omega + 0.005*_omega;
  // PLL gain
  _pllGain = 0.0005;

  // symbol moving average
  _symbols = Buffer<int8_t>(_corrLen);
  for (size_t i=0; i<_corrLen; i++) { _symbols[i] = 0; }
  _symIdx = 0; _symSum  = 0; _lastSymSum = 0;

  // Reset bit hist
  _lastBits = 0;

  // Allocate output buffer
  _buffer = Buffer<uint8_t>(1+src_cfg.bufferSize()/_corrLen);

  LogMessage msg(LOG_DEBUG);
  msg << "Config FSKBitStreamBase node: " << std::endl
      << " input sample rate: " << src_cfg.sampleRate() << " Hz" << std::endl
      << " baud rate: " << _baud << std::endl
      << " samples per bit: " << 1./_omega << std::endl
      << " phase incr/symbol: " << _omega;
  Logger::get().log(msg);

  // Forward config.
  this->setConfig(Config(Traits<uint8_t>::scalarId, _baud, _buffer.size(), 1));
}

void
BitStream::process(const Buffer<uint8_t> &buffer, bool allow_overwrite)
{
  size_t o=0;
  for (size_t i=0; i<buffer.size(); i++)
  {
    // store symbol & update _symSum and _lastSymSum
    _lastSymSum = _symSum;
    _symSum -= _symbols[_symIdx];
    _symbols[_symIdx] = ( buffer[i] ? 1 : -1 );
    _symSum += _symbols[_symIdx];
    _symIdx = ((_symIdx+1) % _corrLen);

    // Advance phase
    _phase += _omega;

    // Sample bit ...
    if (_phase >= 1) {
      // Modulo "2 pi", phase is defined on the interval [0,1)
      while (_phase>=1) { _phase -= 1; }
      // Estimate bit by majority vote on all symbols (_symSum)
      _lastBits = ((_lastBits<<1) | (_symSum>0));
      // Put decoded bit in output buffer
      if (TRANSITION == _mode) {
        // transition -> 0; no transition -> 1
        _buffer[o++] = ((_lastBits ^ (_lastBits >> 1) ^ 0x1) & 0x1);
      } else {
        // mark -> 1, space -> 0
        _buffer[o++] = _lastBits & 0x1;
      }
    }

    // If there was a symbol transition
    if (((_lastSymSum < 0) && (_symSum>=0)) || ((_lastSymSum >= 0) && (_symSum<0))) {
      // Phase correction
      // transition at [-pi,0] -> increase omega
      if (_phase < 0.5) { _omega += _pllGain*(0.5-_phase); }
      // transition at [0,pi]  -> decrease omega
      else { _omega -= _pllGain*(_phase-0.5); }
      // Limit omega
      _omega = std::min(_omegaMax, std::max(_omegaMin, _omega));
    }
  }

  if (o>0) { this->send(_buffer.head(o)); }
}
