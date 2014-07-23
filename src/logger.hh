#ifndef __SDR_LOGGER_HH__
#define __SDR_LOGGER_HH__

#include <string>
#include <sstream>
#include <list>


namespace sdr {

/** Specifies the possible log-level. */
typedef enum {
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARNING,
  LOG_ERROR
} LogLevel;


/** A log message. */
class LogMessage: public std::stringstream
{
public:
  /** Constructor.
   * @param level Specified the log-level of the message.
   * @param msg   An optional message. */
  LogMessage(LogLevel level, const std::string &msg="");
  /** Copy constructor. */
  LogMessage(const LogMessage &other);
  /** Destructor. */
  virtual ~LogMessage();

  /** Returns the level of the message. */
  LogLevel level() const;
  /** Returns the message. */
  inline std::string message() const { return this->str(); }

protected:
  /** The level of the message. */
  LogLevel _level;
};


/** Base class of all log message handlers. */
class LogHandler
{
protected:
  /** Hidden constructor. */
  LogHandler();

public:
  /** Destructor. */
  virtual ~LogHandler();
  /** Needs to be implemented by sub-classes to handle log messages. */
  virtual void handle(const LogMessage &msg) = 0;
};



/** Serializes log message into the specified stream. */
class StreamLogHandler: public LogHandler
{
public:
  /** Constructor.
   * @param stream Specifies the stream, the messages are serialized into.
   * @param level Specifies the minimum log level of the messages being serialized.
   */
  StreamLogHandler(std::ostream &stream, LogLevel level);
  /** Destructor. */
  virtual ~StreamLogHandler();
  /** Handles the message. */
  virtual void handle(const LogMessage &msg);

protected:
  /** The output stream. */
  std::ostream &_stream;
  /** The minimum log-level. */
  LogLevel _level;
};



/** The logger class (singleton). */
class Logger
{
protected:
  /** Hidden constructor. Use @c get to obtain an instance. */
  Logger();

public:
  /** Destructor. */
  virtual ~Logger();

  /** Returns the singleton instance of the logger. */
  static Logger &get();

  /** Logs a message. */
  void log(const LogMessage &message);
  /** Adds a message handler. The ownership of the hander is transferred to the logger
   *  instance. */
  void addHandler(LogHandler *handler);

protected:
  /** The singleton instance. */
  static Logger *_instance;
  /** All registered handlers. */
  std::list<LogHandler *> _handler;
};

}

#endif // __SDR_LOGGER_HH__
