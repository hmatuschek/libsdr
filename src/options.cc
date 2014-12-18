#include "options.hh"
#include <getopt.h>
#include <stdlib.h>
#include <sstream>
#include <string.h>

#include <iostream>

using namespace sdr;


/* ********************************************************************************************* *
 * Implementation of Options
 * ********************************************************************************************* */
Options::Options()
  : _options()
{
  // pass...
}

bool
Options::has(const char *name) {
  return _options.end() != _options.find(name);
}

const Options::Value &
Options::get(const char *name) {
  return _options[name];
}

bool
Options::parse(const Definition defs[], int argc, char *argv[], Options &options)
{
  // Get number of definitions
  const Definition *def = defs;
  int nopts = 0;
  while (def->name) { nopts++; def++;  }

  struct option *getopt_options = (struct option *)malloc((nopts+1)*sizeof(struct option));
  std::map<std::string, Definition> long_options;
  std::map<char, Definition> short_options;

  def = defs; struct option *getopt_opt = getopt_options;
  std::string short_opt_str;
  for (int i=0; i<nopts; i++, def++, getopt_opt++) {
    getopt_opt->name = def->name;
    if (FLAG == def->type) { getopt_opt->has_arg = no_argument; }
    else { getopt_opt->has_arg = required_argument; }
    getopt_opt->flag = 0;
    getopt_opt->val = def->short_name;
    long_options[def->name] = *def;
    if (def->short_name) {
      short_options[def->short_name] = *def;
      short_opt_str += def->short_name;
      if (FLAG != def->type) { short_opt_str += ':'; }
    }
  }
  // add sentinel
  getopt_opt->name = 0; getopt_opt->has_arg = 0; getopt_opt->flag = 0; getopt_opt->val = 0;

  // Parse options using getopt
  while (true) {
    int option_index = 0;
    int c = getopt_long(argc, argv, short_opt_str.c_str(), getopt_options, &option_index);
    if (-1 == c) { break; }

    // Handle long option:
    if (0 == c) {
      const char *name = getopt_options[option_index].name;
      if (long_options.end() == long_options.find(name)) {
        free(getopt_options);
        return false;
      }
      const Definition &opt = long_options[name];
      if (FLAG == opt.type) { options._options[name] = Value(); }
      else if (INTEGER == opt.type) { options._options[name] = atol(optarg); }
      else if (FLOAT == opt.type) { options._options[name] = atof(optarg); }
      else if (ANY == opt.type) { options._options[name] = Value(optarg); }
    } else {
      if (short_options.end() == short_options.find(c)) {
        free(getopt_options);
        return false;
      }
      const Definition &opt = short_options[c];
      const char *name = opt.name;
      if (FLAG == opt.type) { options._options[name] = Value(); }
      else if (INTEGER == opt.type) { options._options[name] = atol(optarg); }
      else if (FLOAT == opt.type) { options._options[name] = atof(optarg); }
      else if (ANY == opt.type) { options._options[name] = Value(optarg); }
    }
  }

  free(getopt_options);
  return true;
}


void
Options::print_help(std::ostream &stream, const Definition defs[])
{
  for (const Definition *def = defs; def->name; def++) {
    stream << "--" << def->name;
    if (def->short_name) { stream << ", -" << def->short_name; }
    if (INTEGER == def->type) { stream << " INTEGER"; }
    else if (FLOAT == def->type) { stream << " FLOAT"; }
    else if (ANY == def->type) { stream << " VALUE"; }
    stream << std::endl;
    if (def->help) {
      std::istringstream iss(def->help);
      std::string line("  ");
      do {
        std::string word; iss >> word;
        if ((line.length()+word.length()) > 78) {
          stream << line << std::endl;
          line = "  ";
        }
        line += word + " ";
      } while (iss);
      if (! line.empty()) { stream << line << std::endl; }
    }
    stream << std::endl;
  }
}



/* ********************************************************************************************* *
 * Implementation of Value
 * ********************************************************************************************* */
Options::Value::Value()
  : _type(NONE)
{
  // pass...
}

Options::Value::Value(long value)
  : _type(INTEGER)
{
  _value.as_int = value;
}

Options::Value::Value(double value)
  : _type(FLOAT)
{
  _value.as_float = value;
}

Options::Value::Value(const std::string &value)
  : _type(STRING)
{
  _value.as_string = strdup(value.c_str());
}

Options::Value::~Value() {
  if (STRING == _type) { free(_value.as_string); }
}

Options::Value::Value(const Value &other)
  : _type(other._type), _value(other._value)
{
  if (STRING == _type) { _value.as_string = strdup(_value.as_string); }
}

const Options::Value &
Options::Value::operator =(const Value &other) {
  if (STRING == _type) { free(_value.as_string); }
  _type = other._type;
  if (NONE == _type) { /* pass...*/ }
  else if (INTEGER == _type) { _value.as_int = other._value.as_int; }
  else if (FLOAT == _type) { _value.as_float = other._value.as_float; }
  else if (STRING == _type) { _value.as_string = strdup(other._value.as_string); }
  return *this;
}

bool
Options::Value::isNone() const {
  return NONE == _type;
}

bool
Options::Value::isInteger() const {
  return INTEGER == _type;
}

bool
Options::Value::isFloat() const {
  return FLOAT == _type;
}

bool
Options::Value::isString() const {
  return STRING == _type;
}

long
Options::Value::toInteger() const {
  return _value.as_int;
}

double
Options::Value::toFloat() const {
  return _value.as_float;
}

std::string
Options::Value::toString() const {
  return _value.as_string;
}
