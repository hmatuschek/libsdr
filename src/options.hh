#ifndef __SDR_OPTIONS_HH__
#define __SDR_OPTIONS_HH__

#include <string>
#include <map>

namespace sdr {

/** Convenience functions for command line arguments. */
class Options
{
public:
  /** The argument value. */
  class Value {
  protected:
    /** Value type. */
    typedef enum {
      NONE,    ///< Empty or invalid value.
      INTEGER, ///< An integer (long int).
      FLOAT,   ///< A floating point number (double).
      STRING   ///< An ASCII string.
    } Type;

  public:
    /** Empty constructor. */
    Value();
    /** Integer constructor. */
    Value(long value);
    /** Floating point constructor. */
    Value(double value);
    /** String constructor. */
    Value(const std::string &value);
    /** Copy constructor. */
    Value(const Value &other);
    /** Destructor. */
    ~Value();
    /** Assignment. */
    const Value &operator=(const Value &other);
    /** Returns @c true if the value is empty. */
    bool isNone() const;
    /** Returns @c true if the value is an integer. */
    bool isInteger() const;
    /** Returns @c true if the value is a floating point number. */
    bool isFloat() const;
    /** Returns @c true if the value is a string. */
    bool isString() const;
    /** Turns the value into an integer. */
    long toInteger() const;
    /** Turns the value into a floating point number. */
    double toFloat() const;
    /** Turns the value into a string. */
    std::string toString() const;

  protected:
    /** The type of the value. */
    Type _type;
    /** Values. */
    union {
      long as_int;
      double as_float;
      char *as_string;
    } _value;
  };

  /** Possible argument types. */
  typedef enum {
    FLAG,     ///< No argument (flag).
    INTEGER,  ///< Integer argument.
    FLOAT,    ///< Floating point argument.
    ANY       ///< Any argument (string).
  } ArgType;

  /** Argument definition. */
  typedef struct {
    const char *name;  ///< Argument name (long).
    char short_name;   ///< Argument name (short).
    ArgType type;      ///< Argument type.
    const char *help;  ///< Help string.
  } Definition;

  /** Parse the given arguments (@c argc, @c argv) using the definitions @c defs and stores
   * the results into @c options. Returns @c false on error. */
  static bool parse(const Definition defs[], int argc, char *argv[], Options &options);
  /** Serializes a help text derived from given the definitions into @c stream. */
  static void print_help(std::ostream &stream, const Definition defs[]);

public:
  /** Empty constructor. */
  Options();
  /** Returns @c true if the specified option was found (long name). */
  bool has(const char *name);
  /** Returns the argument of the specified option (long name). */
  const Value &get(const char *name);

protected:
  /** The table of option names and argument values. */
  std::map<std::string, Value> _options;
};

}

#endif // __SDR_OPTIONS_HH__
