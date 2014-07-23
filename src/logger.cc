#include "logger.hh"

using namespace sdr;


/* ********************************************************************************************* *
 * LogMessage
 * ********************************************************************************************* */
LogMessage::LogMessage(LogLevel level, const std::string &msg)
  : std::stringstream(), _level(level)
{
  (*this) << msg;
}

LogMessage::LogMessage(const LogMessage &other)
  : std::stringstream(), _level(other._level)
{
  (*this) << other.message();
}

LogMessage::~LogMessage() {
  // pass...
}

LogLevel
LogMessage::level() const {
  return _level;
}


/* ********************************************************************************************* *
 * LogHandler
 * ********************************************************************************************* */
LogHandler::LogHandler() {
  // pass...
}

LogHandler::~LogHandler() {
  // pass...
}


/* ********************************************************************************************* *
 * StreamLogHandler
 * ********************************************************************************************* */
StreamLogHandler::StreamLogHandler(std::ostream &stream, LogLevel level)
  : LogHandler(), _stream(stream), _level(level)
{
  // pass...
}

StreamLogHandler::~StreamLogHandler() {
  // pass...
}

void
StreamLogHandler::handle(const LogMessage &msg) {
  if (msg.level() < _level) { return; }
  switch (msg.level()) {
  case LOG_DEBUG: _stream << "DEBUG: "; break;
  case LOG_INFO: _stream << "INFO: "; break;
  case LOG_WARNING: _stream << "WARN: "; break;
  case LOG_ERROR: _stream << "ERROR: "; break;
  }
  _stream << msg.message() << std::endl;
}


/* ********************************************************************************************* *
 * Logger
 * ********************************************************************************************* */
Logger *Logger::_instance = 0;

Logger::Logger()
  : _handler()
{
  // pass...
}

Logger::~Logger() {
  std::list<LogHandler *>::iterator item = _handler.begin();
  for (; item != _handler.end(); item++) {
    delete (*item);
  }
  _handler.clear();
}

Logger &
Logger::get() {
  if (0 == _instance) { _instance = new Logger(); }
  return *_instance;
}

void
Logger::addHandler(LogHandler *handler) {
  _handler.push_back(handler);
}

void
Logger::log(const LogMessage &message) {
  std::list<LogHandler *>::iterator item = _handler.begin();
  for (; item != _handler.end(); item++) {
    (*item)->handle(message);
  }
}
