#include "psk31.hh"
#include "logger.hh"

using namespace sdr;


Varicode::Varicode()
  : Sink<uint8_t>(), Source()
{
  _code_table[1023] = '!';  _code_table[87]   = '.';  _code_table[895]  = '\'';
  _code_table[367]  = '*';  _code_table[495]  = '\\'; _code_table[687]  = '?';
  _code_table[475]  = '$';  _code_table[701]  = '@';  _code_table[365]  = '_';
  _code_table[735]  = '`';  _code_table[351]  = '"';  _code_table[493]  = '<';
  _code_table[727]  = '~';  _code_table[699]  = '&';  _code_table[703]  = '^';
  _code_table[507]  = ']';  _code_table[117]  = '-';  _code_table[445]  = ';';
  _code_table[1013] = '#';  _code_table[695]  = '{';  _code_table[245]  = ':';
  _code_table[693]  = '}';  _code_table[503]  = ')';  _code_table[1749] = '%';
  _code_table[471]  = '>';  _code_table[991]  = '+';  _code_table[251]  = '[';
  _code_table[85]   = '=';  _code_table[943]  = '/';  _code_table[29]   = '\n';
  _code_table[443]  = '|';  _code_table[1]    = ' ';  _code_table[125]  = 'A';
  _code_table[235]  = 'B';  _code_table[173]  = 'C';  _code_table[181]  = 'D';
  _code_table[119]  = 'E';  _code_table[219]  = 'F';  _code_table[253]  = 'G';
  _code_table[341]  = 'H';  _code_table[127]  = 'I';  _code_table[509]  = 'J';
  _code_table[381]  = 'K';  _code_table[215]  = 'L';  _code_table[187]  = 'M';
  _code_table[221]  = 'N';  _code_table[171]  = 'O';  _code_table[213]  = 'P';
  _code_table[477]  = 'Q';  _code_table[175]  = 'R';  _code_table[111]  = 'S';
  _code_table[109]  = 'T';  _code_table[343]  = 'U';  _code_table[437]  = 'V';
  _code_table[349]  = 'W';  _code_table[373]  = 'X';  _code_table[379]  = 'Y';
  _code_table[685]  = 'Z';  _code_table[11]   = 'a';  _code_table[95]   = 'b';
  _code_table[47]   = 'c';  _code_table[45]   = 'd';  _code_table[3]    = 'e';
  _code_table[61]   = 'f';  _code_table[91]   = 'g';  _code_table[43]   = 'h';
  _code_table[13]   = 'i';  _code_table[491]  = 'j';  _code_table[191]  = 'k';
  _code_table[27]   = 'l';  _code_table[59]   = 'm';  _code_table[15]   = 'n';
  _code_table[7]    = 'o';  _code_table[63]   = 'p';  _code_table[447]  = 'q';
  _code_table[21]   = 'r';  _code_table[23]   = 's';  _code_table[5]    = 't';
  _code_table[55]   = 'u';  _code_table[123]  = 'v';  _code_table[107]  = 'w';
  _code_table[223]  = 'x';  _code_table[93]   = 'y';  _code_table[469]  = 'z';
  _code_table[183]  = '0';  _code_table[445]  = '1';  _code_table[237]  = '2';
  //                          ^- Collides with ';'!
  _code_table[511]  = '3';  _code_table[375]  = '4';  _code_table[859]  = '5';
  _code_table[363]  = '6';  _code_table[941]  = '7';  _code_table[427]  = '8';
  _code_table[951]  = '9';
}

Varicode::~Varicode() {
  // pass...
}

void
Varicode::config(const Config &src_cfg) {
  // Requires type, sample rate & buffer size
  if (!src_cfg.hasType() || !src_cfg.hasBufferSize()) { return; }
  // Check buffer type
  if (Config::typeId<uint8_t>() != src_cfg.type()) {
    ConfigError err;
    err << "Can not configure Varicode: Invalid type " << src_cfg.type()
        << ", expected " << Config::typeId<uint8_t>();
    throw err;
  }

  _value = 0;
  _buffer = Buffer<uint8_t>(18);
  this->setConfig(Config(Traits<uint8_t>::scalarId, 0, 18, 1));
}

void
Varicode::process(const Buffer<uint8_t> &buffer, bool allow_overwrite) {
  size_t oidx = 0;
  for (size_t i=0; i<buffer.size(); i++) {
    _value = (_value << 1) | (buffer[i]&0x01);
    if (0 == (_value&0x03)) {
      _value >>= 2;
      if (_value) {
        std::map<uint16_t, char>::iterator item = _code_table.find(_value);
        if (item != _code_table.end()) {
          _buffer[oidx++] = item->second;
        } else {
          LogMessage msg(LOG_INFO);
          msg << "Can not decode varicode " << _value << ": Unkown symbol.";
          Logger::get().log(msg);
        }
      }
      _value = 0;
    }
  }
  if (oidx) {
    this->send(_buffer.head(oidx));
  }
}

