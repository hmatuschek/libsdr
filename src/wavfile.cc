#include "wavfile.hh"
#include "config.hh"
#include "logger.hh"
#include <cstring>


using namespace sdr;

WavSource::WavSource(size_t buffer_size)
  : Source(), _file(), _buffer(), _buffer_size(buffer_size),
    _frame_count(0), _type(Config::Type_UNDEFINED), _sample_rate(0), _frames_left(0)
{
  // pass..
}

WavSource::WavSource(const std::string &filename, size_t buffer_size)
  : Source(), _file(), _buffer(), _buffer_size(buffer_size),
    _frame_count(0), _type(Config::Type_UNDEFINED), _sample_rate(0), _frames_left(0)
{
  open(filename);
}

WavSource::~WavSource() {
  _file.close();
}

bool
WavSource::isOpen() const {
  return _file.is_open();
}

void
WavSource::open(const std::string &filename)
{
  if (_file.is_open()) { _file.close(); }
  _file.open(filename.c_str(), std::ios_base::in);
  if (! _file.is_open()) { return; }

  // Read header and configure source
  char str[5]; str[4] = 0;
  uint16_t val2; uint32_t val4;
  uint16_t n_chanels;
  uint32_t sample_rate;
  uint16_t block_align, bits_per_sample;
  uint32_t chunk_offset, chunk_size;

  /*
   * Read Header
   */
  chunk_offset = 0;
  _file.read(str, 4);
  if (0 != strncmp(str, "RIFF", 4)) {
    RuntimeError err;
    err << "File '" << filename << "' is not a WAV file.";
    throw err;
  }

  _file.read((char *)&chunk_size, 4); // Read file-size (unused)
  _file.read(str, 4);  // Read "WAVE" (unused)
  if (0 != strncmp(str, "WAVE", 4)) {
    RuntimeError err;
    err << "File '" << filename << "' is not a WAV file.";
    throw err;
  }


  /*
   * Read Format part
   */
  chunk_offset = 12;
  _file.read(str, 4);   // Read "fmt " (unused)
  if (0 != strncmp(str, "fmt ", 4)) {
    RuntimeError err;
    err << "'File 'fmt' header missing in file " << filename << "' @" << _file.tellg();
    throw err;
  }
  _file.read((char *)&chunk_size, 4); // Subheader size (unused)

  _file.read((char *)&val2, 2); // Format
  if (1 != val2) {
    RuntimeError err;
    err << "Unsupported WAV data format: " << val2
        << " of file " << filename << ". Expected " << 1;
    throw err;
  }

  _file.read((char *)&n_chanels, 2); // Read # chanels
  if ((1 != n_chanels) && (2 != n_chanels)) {
    RuntimeError err;
    err << "Unsupported number of chanels: " << n_chanels
        << " of file " << filename << ". Expected 1 or 2.";
    throw err;
  }

  _file.read((char *)&sample_rate, 4); // Read sample-rate
  _file.read((char *)&val4, 4);        // Read byte-rate (unused)
  _file.read((char *)&block_align, 2);
  _file.read((char *)&bits_per_sample, 2);

  // Check sample format
  if ((16 != bits_per_sample) && (8 != bits_per_sample)){
    RuntimeError err;
    err << "Unsupported sample format: " << bits_per_sample
        << "b of file " << filename << ". Expected 16b or 8b.";
    throw err;
  }
  if (block_align != n_chanels*(bits_per_sample/8)) {
    RuntimeError err;
    err << "Unsupported alignment: " << block_align
        << "byte of file " << filename << ". Expected " << (bits_per_sample/8) << "byte.";
    throw err;
  }
  // Seek to end of header
  chunk_offset += 8+chunk_size;
  _file.seekg(chunk_offset);

  // Search for data chunk
  _file.read(str, 4);
  while ((0 != strncmp(str, "data", 4)) || _file.eof()) {
    // read chunk size
    _file.read((char*)&chunk_size, 4);
    chunk_offset += (8+chunk_size);
    _file.seekg(chunk_offset);
    _file.read(str, 4);
  }
  if (_file.eof()) {
    RuntimeError err;
    err << "WAV file '" << filename << "' contains no 'data' chunk.";
    throw err;
  }

  /*
   * Read data part.
   */
  _file.read((char *)&chunk_size, 4); // read frame count

  // Configure source
  _frame_count = chunk_size/(2*n_chanels);
  if ((1 == n_chanels) && (8 == bits_per_sample)) { _type = Config::Type_u8; }
  else if ((1==n_chanels) && (16 == bits_per_sample)) { _type = Config::Type_s16; }
  else if ((2==n_chanels) && ( 8 == bits_per_sample)) { _type = Config::Type_cu8; }
  else if ((2==n_chanels) && (16 == bits_per_sample)) { _type = Config::Type_cs16; }
  else {
    ConfigError err; err << "Can not configure WavSource: Unsupported PCM type."; throw err;
  }

  _sample_rate = sample_rate;
  _frames_left = _frame_count;

  LogMessage msg(LOG_DEBUG);
  msg << "Configured WavSource:" << std::endl
      << " file: " << filename << std::endl
      << " type:"  << _type << std::endl
      << " sample-rate: " << _sample_rate << std::endl
      << " frame-count: " << _frame_count << std::endl
      << " duration: " << _frame_count/_sample_rate << "s" << std::endl
      << " buffer-size: " << _buffer_size;
  Logger::get().log(msg);

  // unreference buffer if not empty
  if (! _buffer.isEmpty()) { _buffer.unref(); }

  // Allocate buffer and propergate config
  switch (_type) {
  case Config::Type_u8:
    _buffer = Buffer<uint8_t>(_buffer_size);
    this->setConfig(Config(Config::Type_u8, _sample_rate, _buffer_size, 1));
    break;
  case Config::Type_s16:
    _buffer = Buffer<int16_t>(_buffer_size);
    this->setConfig(Config(Config::Type_s16, _sample_rate, _buffer_size, 1));
    break;
  case Config::Type_cu8:
    _buffer = Buffer< std::complex<uint8_t> >(_buffer_size);
    this->setConfig(Config(Config::Type_cu8, _sample_rate, _buffer_size, 1));
    break;
  case Config::Type_cs16:
    _buffer = Buffer< std::complex<int16_t> >(_buffer_size);
    this->setConfig(Config(Config::Type_cs16, _sample_rate, _buffer_size, 1));
    break;
  default: {
    ConfigError err; err << "Can not configure WavSource: Unsupported PCM type."; throw err;
  }
  }
}

void
WavSource::close() {
  _file.close(); _frames_left = 0;
}


bool
WavSource::isReal() const {
  return (Config::Type_u8 == _type) || (Config::Type_s16 == _type);
}

void
WavSource::next()
{
  //Logger::get().log(LogMessage(LOG_DEBUG, "WavSource: Read next buffer"));

  if ((0 == _frames_left)) {
    // Close file
    _file.close();
    Logger::get().log(LogMessage(LOG_DEBUG, "WavSource: End of file -> stop queue."));
    // and signal queue to stop
    Queue::get().stop();
    return;
  }

  // Determine the number of frames to read
  size_t n_frames = std::min(_frames_left, _buffer_size);

  switch (_type) {
  case Config::Type_u8:
    _file.read(_buffer.ptr(), n_frames*sizeof(uint8_t));
    _frames_left -= n_frames;
    this->send(RawBuffer(_buffer, 0, n_frames*sizeof(uint8_t)), true);
    break;
  case Config::Type_s16:
    _file.read(_buffer.ptr(), n_frames*sizeof(int16_t));
    _frames_left -= n_frames;
    this->send(RawBuffer(_buffer, 0, n_frames*sizeof(int16_t)), true);
    break;
  case Config::Type_cu8:
    _file.read(_buffer.ptr(), 2*n_frames*sizeof(uint8_t));
    _frames_left -= n_frames;
    this->send(RawBuffer(_buffer, 0, 2*n_frames*sizeof(uint8_t)), true);
    break;
  case Config::Type_cs16:
    _file.read(_buffer.ptr(), 2*n_frames*sizeof(int16_t));
    _frames_left -= n_frames;
    this->send(RawBuffer(_buffer, 0, 2*n_frames*sizeof(int16_t)), true);
    break;
  default:
    break;
  }
}

