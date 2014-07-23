#ifndef __SDR_EXCEPTION_HH__
#define __SDR_EXCEPTION_HH__

#include <exception>
#include <sstream>

namespace sdr {

/** Base class of all SDR exceptions. */
class SDRError : public std::exception, public std::stringstream {
public:
  /** Constructor. */
  SDRError(): std::exception(), std::stringstream() { }
  /** Copy constructor. */
  SDRError(const SDRError &other)
    : std::exception(), std::stringstream() { this->str(other.str()); }
  /** Destructor. */
  virtual ~SDRError() throw() { }
  /** Implements the @c std::exception interface. */
  virtual const char *what() const throw() { return this->str().c_str(); }
};

/** The configuration error class. */
class ConfigError : public SDRError {
public:
  /** Constructor. */
  ConfigError(): SDRError() {}
  /** Copy constructor. */
  ConfigError(const ConfigError &other): SDRError(other) {}
  /** Destructor. */
  virtual ~ConfigError() throw() { }
};


/** The runtime error class. */
class RuntimeError: public SDRError {
public:
  /** Constructor. */
  RuntimeError(): SDRError() {}
  /** Copy constructor. */
  RuntimeError(const RuntimeError &other): SDRError(other) {}
  /** Destructor. */
  virtual ~RuntimeError() throw() { }
};



}
#endif // __SDR_EXCEPTION_HH__
