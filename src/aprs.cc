#include "aprs.hh"
#include "logger.hh"

using namespace sdr;



/* ******************************************************************************************** *
 * Implementation of APRS
 * ******************************************************************************************** */
APRS::APRS()
  : AX25()
{
  // pass...
}

void
APRS::handleAX25Message(const AX25::Message &message) {
  // Skip non-UI frames
  if (0x03 != uint8_t(message.payload()[0])) {
    LogMessage msg(LOG_DEBUG);
    msg << "APRS: Skip non-UI frame (type="
        << std::hex << int(uint8_t(message.payload()[0]))
        << std::dec << "): " << message;
    Logger::get().log(msg);
    return;
  }
  // Skip frames woth level-3 protocol
  if (0xf0 != uint8_t(message.payload()[1])) {
    LogMessage msg(LOG_DEBUG);
    msg << "APRS: Skip invalid UI (pid="
        << std::hex << int(uint8_t(message.payload()[1]))
        << std::dec << "): " << message;
    Logger::get().log(msg);
    return;
  }
  // Construct APRS message
  Message msg(message);
  // Handle message
  this->handleAPRSMessage(msg);
}

void
APRS::handleAPRSMessage(const Message &message) {
  std::cerr << message << std::endl;
}


/* ******************************************************************************************** *
 * Implementation of APRS::Message
 * ******************************************************************************************** */
bool __is_number(char c) {
  return (('0'<=c) && ('9'>=c));
}

APRS::Message::Symbol
__toSymbol(char table, char sym) {
  if ('/' == table) {
    switch (sym) {
    case 'P':
    case '!': return APRS::Message::POLICE;
    case '%':
    case '&':
    case '(':
    case 'B':
    case 'n':
    case '#': return APRS::Message::DIGI;
    case '[':
    case 'e':
    case '$': return APRS::Message::JOGGER;
    case 'X':
    case '^':
    case 'g':
    case '\'': return APRS::Message::AIRCRAFT;
    case '-': return APRS::Message::HOUSE;
    case 'b':
    case '<': return APRS::Message::MOTORCYCLE;
    case '=':
    case '*':
    case 'U':
    case 'j':
    case 'k':
    case 'u':
    case 'v':
    case '>': return APRS::Message::CAR;
    case 'Y':
    case 's':
    case 'C': return APRS::Message::BOAT;
    case 'O': return APRS::Message::BALLOON;
    case '_': return APRS::Message::WX;
    default: break;
    }
  } else if ('\\' == table) {
    switch (sym) {
    default: break;
    }
  }
  return APRS::Message::NONE;
}


APRS::Message::Message()
  : AX25::Message(), _hasLocation(false), _latitude(0), _longitude(0), _symbol(NONE),
    _hasTime(false), _time(::time(0))
{
  // pass...
}

APRS::Message::Message(const AX25::Message &msg)
  : AX25::Message(msg),
    _hasLocation(false), _latitude(0), _longitude(0), _symbol(NONE),
    _hasTime(false), _time(::time(0))
{
  size_t offset = 2;
  // Dispatch by message type
  switch (_payload[offset]) {
  case '=':
  case '!':
    _hasLocation = true;
    offset++;
    break;
  case '/':
  case '@':
    _hasTime = true;
    _hasLocation = true;
    offset++;
    break;
  case ';':
    _hasTime = true;
    _hasLocation = true;
    // Skip ';', 9 char of object identifier and delimiter ('*' or '_').
    offset += 11;
    break;
  default:
    // On unknown message type -> store complete message as comment
    if (offset < _payload.size()) {
      _comment = _payload.substr(offset);
      offset += _comment.size();
    }
    return;
  }

  if (_hasTime) {
    if (! _readTime(offset)) { return; }
  }
  if (_hasLocation) {
    if (! _readLocation(offset)) { return; }
  }
  // Remaining text is comment
  if (offset < _payload.size()) {
    _comment = _payload.substr(offset);
    offset += _comment.size();
  }
}

bool
APRS::Message::_readLocation(size_t &offset) {
  // Read latitude
  if(! _readLatitude(offset)) { return false; }

  // Read symbol table
  char symbolTable = _payload[offset]; offset++;

  // Read longitude
  if (! _readLongitude(offset)) { return false; }

  // Read symbol
  _symbol = __toSymbol(symbolTable, _payload[offset]); offset++;

  return true;
}


bool
APRS::Message::_readLatitude(size_t &offset)
{
  // read degree
  if (! __is_number(_payload[offset])) { return false; }
  _latitude = int(_payload[offset]-0x30); offset++;
  if (! __is_number(_payload[offset])) { return false; }
  _latitude = _latitude*10 + int(_payload[offset]-0x30); offset++;

  // read mintues
  if (! __is_number(_payload[offset])) { return false; }
  double min = int(_payload[offset]-0x30); offset++;
  if (! __is_number(_payload[offset])) { return false; }
  min = min*10 + int(_payload[offset]-0x30); offset++;

  // check for '.'
  if ('.' != _payload[offset]) { return false; } offset++;

  // read minutes decimal
  if (! __is_number(_payload[offset])) { return false; }
  double mindec = int(_payload[offset]-0x30); offset++;
  if (! __is_number(_payload[offset])) { return false; }
  mindec = mindec*10 + int(_payload[offset]-0x30); offset++;
  min += mindec/100;

  // Update degree
  _latitude += min/60;

  // Process north/south indicator
  switch (_payload[offset]) {
  case 'N': offset++; break;
  case 'S': _latitude = -_latitude; offset++; break;
  default: return false; // <- on invalid indicator.
  }

  // done.
  return true;
}


bool
APRS::Message::_readLongitude(size_t &offset) {
  // read degree
  if (! __is_number(_payload[offset])) { return false; }
  _longitude = int(_payload[offset]-0x30); offset++;
  if (! __is_number(_payload[offset])) { return false; }
  _longitude = _longitude*10 + int(_payload[offset]-0x30); offset++;
  if (! __is_number(_payload[offset])) { return false; }
  _longitude = _longitude*10 + int(_payload[offset]-0x30); offset++;

  // read minutes
  if (! __is_number(_payload[offset])) { return false; }
  double min = int(_payload[offset]-0x30); offset++;
  if (! __is_number(_payload[offset])) { return false; }
  min = min*10 + int(_payload[offset]-0x30); offset++;

  // skip '.'
  if ('.' != _payload[offset]) { return false; } offset++;

  // Read minutes decimals
  if (! __is_number(_payload[offset])) { return false; }
  double mindec = int(_payload[offset]-0x30); offset++;
  if (! __is_number(_payload[offset])) { return false; }
  mindec = mindec*10 + int(_payload[offset]-0x30); offset++;
  min += mindec/100;

  // Update longitude
  _longitude += min/60;

  // read east/west indicator
  switch (_payload[offset]) {
  case 'E': offset++; break;
  case 'W': offset++; _longitude = -_longitude; break;
  default: return false;
  }

  return true;
}


bool
APRS::Message::_readTime(size_t &offset) {
  if (! __is_number(_payload[offset])) { return false; }
  int a = int(_payload[offset]-0x30); offset++;
  if (! __is_number(_payload[offset])) { return false; }
  a = a*10 + int(_payload[offset]-0x30); offset++;

  if (! __is_number(_payload[offset])) { return false; }
  int b = int(_payload[offset]-0x30); offset++;
  if (! __is_number(_payload[offset])) { return false; }
  b = b*10 + int(_payload[offset]-0x30); offset++;

  if (! __is_number(_payload[offset])) { return false; }
  int c = int(_payload[offset]-0x30); offset++;
  if (! __is_number(_payload[offset])) { return false; }
  c = c*10 + int(_payload[offset]-0x30); offset++;

  // Determine type of date-time
  if ('z' == _payload[offset]) {
    // Alter current time in terms of UTC
    struct tm timeStruct = *gmtime(&_time);
    timeStruct.tm_mday = a;
    timeStruct.tm_hour = b;
    timeStruct.tm_min  = c;
    // Update time-stamp
    _time = mktime(&timeStruct);
    offset++;
  } else if ('/' == _payload[offset]) {
    // Alter current time in terms of local time
    struct tm timeStruct = *localtime(&_time);
    timeStruct.tm_mday = a;
    timeStruct.tm_hour = b;
    timeStruct.tm_min  = c;
    // Update time-stamp
    _time = mktime(&timeStruct);
    offset++;
  } else if ('h' == _payload[offset]) {
    // Alter current time in terms of local time
    struct tm timeStruct = *localtime(&_time);
    timeStruct.tm_hour = a;
    timeStruct.tm_min  = b;
    timeStruct.tm_sec  = c;
    // Update time-stamp
    _time = mktime(&timeStruct);
    offset++;
  } else if (('0' <= _payload[offset]) && (('9' >= _payload[offset]))) {
    if (! __is_number(_payload[offset])) { return false; }
    int d = int(_payload[offset]-0x30); offset++;
    if (! __is_number(_payload[offset])) { return false; }
    d = d*10 + int(_payload[offset]-0x30); offset++;
    // Alter current time in terms of local time
    struct tm timeStruct = *localtime(&_time);
    timeStruct.tm_mon  = a;
    timeStruct.tm_mday = b;
    timeStruct.tm_hour = c;
    timeStruct.tm_min  = d;
    // Update time-stamp
    _time = mktime(&timeStruct);
  } else {
    return false;
  }

  // done...
  return true;
}


std::ostream &
sdr::operator <<(std::ostream &stream, const APRS::Message &message) {
  std::cerr << "APRS: " << message.from() << " > " << message.to();
  if (message.via().size()) {
    std::cerr << " via " << message.via()[0];
    for (size_t i=1; i<message.via().size(); i++) {
      std::cerr << ", " << message.via()[i];
    }
  }
  std::cerr << std::endl << " time: " << asctime(localtime(&message.time()));
  if (message.hasLocation()) {
    std::cerr << " location: " << message.latitude() << ", " << message.longitude()
              << std::endl << " symbol: " << message.symbol() << std::endl;
  }
  if (message.hasComment()) {
    std::cerr << " comment: " << message.comment() << std::endl;
  }
  return stream;
}
