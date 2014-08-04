#include "node.hh"

using namespace sdr;


/* ********************************************************************************************* *
 * Implementation of Config class
 * ********************************************************************************************* */
Config::Config()
  : _type(Type_UNDEFINED), _sampleRate(0), _bufferSize(0), _numBuffers(0)
{
  // pass...
}

Config::Config(Type type, double sampleRate, size_t bufferSize, size_t numBuffers)
  : _type(type), _sampleRate(sampleRate), _bufferSize(bufferSize), _numBuffers(numBuffers)
{
  // pass...
}

Config::Config(const Config &other)
  : _type(other._type), _sampleRate(other._sampleRate),
    _bufferSize(other._bufferSize), _numBuffers(other._numBuffers)
{
  // pass...
}

const Config &
Config::operator =(const Config &other) {
  _type = other._type; _sampleRate = other._sampleRate;
  _bufferSize = other._bufferSize; _numBuffers = other._numBuffers;
  return *this;
}

bool
Config::operator ==(const Config &other) const {
  return (other._type == _type) && (other._sampleRate == _sampleRate) &&
      (other._bufferSize == _bufferSize) && (other._numBuffers == _numBuffers);
}


/* ********************************************************************************************* *
 * Implementation of SinkBase
 * ********************************************************************************************* */
SinkBase::SinkBase() {
  // pass...
}

SinkBase::~SinkBase() {
  // pass...
}

/* ********************************************************************************************* *
 * Implementation of Source class
 * ********************************************************************************************* */
Source::Source()
  : _config(), _sinks()
{
  // pass..
}

Source::~Source() {
  // pass...
}

void
Source::send(const RawBuffer &buffer, bool allow_overwrite) {
  std::map<SinkBase *, bool>::iterator item = _sinks.begin();
  for (; item != _sinks.end(); item++) {
    // If connected directly, call directly
    if (item->second) {
      // The source allows the sink to manipulate the buffer directly,
      // iff, the sink is the only one receiving this buffer, the source allows it and the
      // connection is direct.
      allow_overwrite = allow_overwrite && (1 == _sinks.size());
      // Call sink directly
      item->first->handleBuffer(buffer, allow_overwrite);
    }
    else {
      // otherwise, queue buffer
      allow_overwrite = allow_overwrite && (1 == _sinks.size());
      Queue::get().send(buffer, item->first, allow_overwrite);
    }
  }
}

void
Source::connect(SinkBase *sink, bool direct) {
  _sinks[sink] = direct;
  sink->config(_config);
}

void
Source::disconnect(SinkBase *sink) {
  _sinks.erase(sink);
}

void
Source::setConfig(const Config &config) {
  // If the config did not changed -> skip
  if (config == _config) { return; }
  // Store config
  _config = config;
  // Propergate config
  propagateConfig(_config);
}

void
Source::propagateConfig(const Config &config) {
  // propagate config to all connected sinks
  std::map<SinkBase *, bool>::iterator item = _sinks.begin();
  for (; item != _sinks.end(); item++) {
    item->first->config(_config);
  }
}

Config::Type
Source::type() const {
  return _config.type();
}

double
Source::sampleRate() const {
  return _config.sampleRate();
}

void
Source::signalEOS() {
  std::list<DelegateInterface *>::iterator item = _eos.begin();
  for (; item != _eos.end(); item++) { (**item)(); }
}



/* ********************************************************************************************* *
 * Implementation of BlockingSource
 * ********************************************************************************************* */
BlockingSource::BlockingSource(bool parallel, bool connect_idle, bool stop_queue_on_eos)
  : Source(), _is_active(false), _is_parallel(parallel)
{
  // If the source is not autonomous and shall be triggered by the idle event of the Queue
  //   -> connect to it.
  if (!parallel && connect_idle) {
    Queue::get().addIdle(this, &BlockingSource::_nonvirt_idle_cb);
  }
  // If the EOS signal of the source shall stop the queue
  if (stop_queue_on_eos) { this->addEOS(&Queue::get(), &Queue::stop); }
}

BlockingSource::~BlockingSource() {
  if (isActive()) { stop(); }
}

void
BlockingSource::start() {
  if (_is_active) { return; }
  if (_is_parallel) {
    pthread_create(&_thread, 0, BlockingSource::_pthread_main_wrapper, this);
  }
}

void
BlockingSource::stop() {
  if (! _is_active) { return; }
  _is_active = false;
  if (_is_parallel) {
    void *ret_ptr;
    pthread_join(_thread, &ret_ptr);
  }
}

void
BlockingSource::_parallel_main() {
  while (_is_active && Queue::get().isRunning()) {
    this->next();
  }
}

void
BlockingSource::_nonvirt_idle_cb() {
  if (_is_active && Queue::get().isRunning()) {
    this->next();
  }
}

void *
BlockingSource::_pthread_main_wrapper(void *ptr) {
  BlockingSource *obj = reinterpret_cast<BlockingSource *>(ptr);
  obj->_parallel_main();
  return 0;
}



/* ********************************************************************************************* *
 * Implementation of Proxy
 * ********************************************************************************************* */
Proxy::Proxy()
  : SinkBase(), Source()
{
  // pass...
}

Proxy::~Proxy() {
  // pass...
}

void
Proxy::config(const Config &src_cfg) {
  this->setConfig(src_cfg);
}

void
Proxy::handleBuffer(const RawBuffer &buffer, bool allow_overwrite) {
  this->send(buffer);
}
