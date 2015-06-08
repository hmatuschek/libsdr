#include "http.hh"
#include "exception.hh"
#include "logger.hh"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <iomanip>

using namespace sdr;
using namespace sdr::http;


/* ********************************************************************************************* *
 * Utility functions
 * ********************************************************************************************* */
inline bool is_cr(char c) {
  return ('\r'==c);
}

inline bool is_nl(char c) {
  return ('\n'==c);
}

inline bool is_colon(char c) {
  return (':'==c);
}

inline bool is_alpha(char c) {
  return ( ((c>='A') && (c<='Z')) || ((c>='a') && (c<='z')) );
}

inline bool is_num(char c) {
  return ((c>='0') && (c<='9'));
}

inline bool is_ws(char c) {
  return ((' '==c) || ('\t'==c) || ('\n'==c) || ('\r'==c));
}

inline bool is_alpha_num(char c) {
  return (is_alpha(c) || is_num(c));
}

inline bool is_id_start(char c) {
  return (is_alpha(c) || '_');
}

inline bool is_id_part(char c) {
  return (is_alpha_num(c) || '_');
}

inline bool is_space(char c) {
  return (' ' == c);
}

inline bool is_header_part(char c) {
  return (is_alpha_num(c) || ('-' == c) || ('_' == c));
}

inline bool is_header_value_part(char c) {
  return ((c>=32) && (c<=127));
}

inline bool is_url_unreserved(char c) {
  return (is_alpha_num(c) || ('-'==c) || ('_'==c) || ('.'==c) || ('~'==c));
}

inline bool is_url_reserved(char c) {
  return (('!'==c) || ('*'==c) || ('\''==c) || ('('==c) || (')'==c) ||
          (';'==c) || (':'==c) || ('@'==c) || ('&'==c) || ('='==c) ||
          ('+'==c) || ('$'==c) || (','==c) || ('/'==c) || ('?'==c) ||
          ('#'==c) || ('['==c) || (']'==c) || ('%'==c));
}

inline bool is_url_part(char c) {
  return  is_url_unreserved(c) || is_url_reserved(c);
}

inline bool is_http_version_part(char c) {
  return ( is_alpha_num(c) || ('/'==c) || ('.'==c));
}

inline http::Method to_method(const std::string &method) {
  if ("GET" == method) { return http::HTTP_GET; }
  else if ("HEAD" == method) { return http::HTTP_HEAD; }
  else if ("POST" == method) { return http::HTTP_POST; }
  return http::HTTP_UNKNOWN;
}

inline http::Version to_version(const std::string &version) {
  if ("HTTP/1.0" == version) { return http::HTTP_1_0; }
  else if ("HTTP/1.1" == version) { return http::HTTP_1_1; }
  return http::UNKNOWN_VERSION;
}


/* ********************************************************************************************* *
 * Implementation of Server
 * ********************************************************************************************* */
Server::Server(uint port)
  : _port(port), _socket(-1)
{
  // pass...
}

Server::~Server() {
  _is_running = false;
  // Close all connections
  std::set<Connection *>::iterator con = _connections.begin();
  for (; con != _connections.end(); con++) {
    // Wait for the connection to close
    (*con)->close(true);
    delete *con;
  }
  // Free all handler
  std::list<Handler *>::iterator item = _handler.begin();
  for (; item != _handler.end(); item++) { delete *item; }
}

void
Server::start(bool wait) {
  _socket = socket(AF_INET, SOCK_STREAM, 0);
  if (_socket < 0) {
    ConfigError err;
    err << "httpd: Error opening socket.";
    throw err;
  }

  struct sockaddr_in serv_addr;
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  serv_addr.sin_port = htons(_port);
  if (bind(_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    ConfigError err;
    err << "httpd: Can bind to address.";
    throw err;
  }

  int reuseaddr = 1;
  if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr))) {
    LogMessage msg(LOG_WARNING);
    msg << "httpd: Can not set SO_REUSEADDR flag for socket.";
    Logger::get().log(msg);
  }

  _is_running = true;
  if (pthread_create(&_thread, 0, &Server::_listen_main, this)) {
    ConfigError err;
    err << "Can not create listen thread.";
    throw err;
  }

  if (wait) { this->wait(); }
}

void
Server::stop(bool wait) {
  _is_running = false;
  // Close all open connections
  std::set<Connection *>::iterator con = _connections.begin();
  for (; con != _connections.end(); con++) {
    (*con)->close(wait);
  }
  // Close the socket we listen on
  ::close(_socket); _socket = -1;
}

void
Server::wait() {
  void *ret=0;
  pthread_join(_thread, &ret);
}

void *
Server::_listen_main(void *ctx) {
  Server *self = (Server *)ctx;
  while (self->_is_running) {
    listen(self->_socket, 5);
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    int socket = accept(self->_socket, (struct sockaddr *) &cli_addr, &clilen);
    if (socket < 0) { continue; }
    try { self->_connections.insert(new Connection(self, socket)); }
    catch (...) { }
    // Free closed connections
    std::set<Connection *>::iterator item = self->_connections.begin();
    for (; item != self->_connections.end(); item++) {
      if ((*item)->isClosed()) { delete *item; item = self->_connections.erase(item); }
    }
  }
  return 0;
}

void
Server::dispatch(const Request &request, Response &response) {
  LogMessage msg(LOG_DEBUG);
  msg << "httpd: ";
  switch (request.method()) {
  case HTTP_GET: msg << "GET "; break;
    case HTTP_HEAD: msg << "HEAD "; break;
    case HTTP_POST: msg << "POST "; break;
    case HTTP_UNKNOWN: msg << "UNKNOWN "; break;
  }
  msg << " " << request.url().toString();
  Logger::get().log(msg);

  std::list<Handler *>::iterator item = _handler.begin();
  for (; item != _handler.end(); item++) {
    if ((*item)->match(request)) {
      (*item)->handle(request, response);
      return;
    }
  }
  response.setStatus(Response::STATUS_NOT_FOUND);
  response.setHeader("Content-length", "0");
  response.sendHeaders();
}

void
Server::addHandler(Handler *handler) {
  _handler.push_back(handler);
}


/* ********************************************************************************************* *
 * Implementation of HTTPD::Connection
 * ********************************************************************************************* */
Connection::Connection(Server *server, int socket)
  : _server(server), _socket(socket)
{
  // Start new thread to parse requests
  if (pthread_create(&_thread, 0, &Connection::_main, (void *)this)) {
    ::close(_socket); _socket = -1;
    RuntimeError err;
    err << "httpd: Can not create thread for connection.";
    throw err;
  }
}

Connection::~Connection() {
  // Close the socket
  this->close(false);
}

void
Connection::close(bool wait) {
  if (-1 != _socket) {
    int socket = _socket; _socket = -1;
    LogMessage msg(LOG_DEBUG);
    msg << "httpd: Close connection " << socket << ".";
    Logger::get().log(msg);
    ::close(socket);
  }
  if (wait && (0 == pthread_kill(_thread, 0))) {
    // Wait for the thread to exit.
    void *ret = 0;
    pthread_join(_thread, &ret);
  }
}

bool
Connection::isClosed() const {
  return ((-1 == _socket) && (0 != pthread_kill(_thread, 0)));
}

void *
Connection::_main(void *ctx)
{
  Connection *self = (Connection *)ctx;
  // While socket is open
  int error = 0; socklen_t errorlen = sizeof (error);
  while (0 == getsockopt(self->_socket, SOL_SOCKET, SO_ERROR, &error, &errorlen)) {
    Request request(self->_socket);
    Response response(self->_socket);
    // Parse request
    if (!request.parse()) {
      // On parser error or no keep-alive -> exit
      self->close(); return 0;
    }
    // on success -> dispatch request by server
    self->_server->dispatch(request, response);
    // If the connection is kept alive -> continue
    if ((! request.isKeepAlive()) || response.closeConnection()) {
      // Signal server to close connection
      self->close(self); return 0;
    }
  }
  // Signal server to close connection
  self->close(self); return 0;
}


/* ********************************************************************************************* *
 * Implementation of HTTPD::URL
 * ********************************************************************************************* */
URL::URL()
  : _protocol(), _host(), _path(), _query()
{
  // pass...
}

URL::URL(const std::string &proto, const std::string &host, const std::string &path)
  : _protocol(proto), _host(host), _path(path)
{
  // pass...
}

URL::URL(const URL &other)
  : _protocol(other._protocol), _host(other._host), _path(other._path), _query(other._query)
{
  // pass...
}

URL &
URL::operator =(const URL &other) {
  _protocol = other._protocol;
  _host     = other._host;
  _path     = other._path;
  _query    = other._query;
  return *this;
}

URL
URL::fromString(const std::string &url)
{
  std::string text(url), proto, host, path, query_str;

  size_t      idx = 0;
  // split proto
  idx = text.find("://");
  if ((0 != idx) && (std::string::npos != idx)) {
    proto = text.substr(0, idx);
    text  = text.substr(idx+3);
  }

  // split host
  if (text.size() && ('/' != text[0])) {
    idx = text.find('/');
    if (std::string::npos != idx) {
      host = text.substr(0, idx);
      path = text.substr(idx);
    }
  } else {
    path = text;
  }

  // split query
  idx = path.find('?');
  if (std::string::npos != idx) {
    query_str = path.substr(idx+1);
    path = path.substr(0, idx);
  }

  URL res(proto, host, path);
  while (query_str.size()) {
    idx = query_str.find('&');
    std::string pair;
    if (std::string::npos != idx) {
      pair = query_str.substr(0, idx);
      query_str = query_str.substr(idx+1);
    } else {
      pair = query_str;
      query_str.clear();
    }
    idx = pair.find("=");
    if (std::string::npos == idx) {
      res.addQuery(pair, "");
    } else {
      res.addQuery(pair.substr(0, idx), pair.substr(idx+1));
    }
  }

  return res;
}


std::string
URL::toString() const {
  std::stringstream buffer;
  // serialize protocol if present
  if (_protocol.size()) { buffer << _protocol << "://"; }
  // serialize host if present
  if (_host.size()) { buffer << _host; }
  // serialize path (even if not present)
  if (_path.size()) { buffer << _path; }
  else { buffer << "/"; }
  // serialize query
  if (_query.size()) {
    buffer << "?";
    std::list< std::pair<std::string, std::string> >::const_iterator pair = _query.begin();
    buffer << encode(pair->first);
    if (pair->second.size()) { buffer << "=" << encode(pair->second); }
    pair++;
    for (; pair != _query.end(); pair++) {
      buffer << "&" << encode(pair->first);
      if (pair->second.size()) { buffer << "=" << encode(pair->second); }
    }
  }
  return buffer.str();
}

std::string
URL::encode(const std::string &str) {
  std::stringstream buffer;
  for (size_t i=0; i<str.size(); i++) {
    if (32 > str[i]) {
      buffer << "%" << std::setw(2) << std::setfill('0') << std::hex << uint(str[i]);
    } else if (127 > str[i]) {
      buffer << "%" << std::setw(2) << std::setfill('0') << std::hex << uint(str[i]);
    } else if (is_url_reserved(str[i])) {
      buffer << "%" << std::setw(2) << std::setfill('0') << std::hex << uint(str[i]);
    } else {
      buffer << str[i];
    }
  }
  return buffer.str();
}

std::string
URL::decode(const std::string &str) {
  std::stringstream buffer;
  for (size_t i=0; i<str.size(); i++) {
    // If there is a %XX string
    if (('%' == str[i]) && (3 <= (str.size()-i))) {
      buffer << char(strtol(str.substr(i+1, 2).c_str(), 0, 16));
    } else { buffer << str[i]; }
  }
  return buffer.str();
}


/* ********************************************************************************************* *
 * Implementation of HTTPD::Request
 * ********************************************************************************************* */
typedef enum {
  READ_METHOD,
  START_URL, READ_URL,
  START_HTTP_VERSION, READ_HTTP_VERSION,
  REQUEST_END,
  START_HEADER, READ_HEADER, START_HEADER_VALUE, READ_HEADER_VALUE, END_HEADER,
  END_HEADERS
} HttpRequestParserState;


Request::Request(int socket)
  : _socket(socket), _method(HTTP_UNKNOWN)
{
  // pass...
}

bool
Request::parse() {
  char c;
  std::stringstream buffer;
  std::string current_header_name;
  HttpRequestParserState state = READ_METHOD;

  // while getting a char from stream
  while (::read(_socket, &c, 1)) {
    switch (state) {
    case READ_METHOD:
      if (is_space(c)) {
        // Get method enum
        _method = to_method(buffer.str());
        if (HTTP_UNKNOWN == _method) {
          LogMessage msg(LOG_DEBUG);
          msg << "http: Got unexpected method '"<<buffer.str() << "'.";
          Logger::get().log(msg);
          return false;
        }
        state = START_URL; buffer.str("");
      } else if (is_alpha_num(c)) {
        buffer << c;
      } else {
        return false;
      }
      break;

    case START_URL:
      if (is_space(c)) {
        continue;
      } else if (is_url_part(c)) {
        state = READ_URL; buffer << c;
      } else {
        return false;
      }
      break;

    case READ_URL:
      if (is_space(c)) {
        state = START_HTTP_VERSION;
        _url = URL::fromString(buffer.str()); buffer.str("");
      } else if (is_url_part(c)) {
        buffer << c;
      } else {
        return false;
      }
      break;

    case START_HTTP_VERSION:
      if (is_space(c)) {
        continue;
      } else if (is_http_version_part(c)) {
        buffer << c; state = READ_HTTP_VERSION;
      } else {
        return false;
      }
      break;

    case READ_HTTP_VERSION:
      if (is_cr(c)) {
        _version = to_version(buffer.str()); buffer.str();
        if (UNKNOWN_VERSION == _version) {
          LogMessage msg(LOG_DEBUG);
          msg << "http: Got invalid version '"<< buffer.str() << "'.";
          Logger::get().log(msg);
          return false;
        }
        state = REQUEST_END;
      } else if (is_http_version_part(c)) {
        buffer << c;
      } else {
        return false;
      }
      break;

    case REQUEST_END:
      if (is_nl(c)) {
        state = START_HEADER;
      } else {
        return false;
      }
      break;

    case START_HEADER:
      if (is_cr(c)) {
        state = END_HEADERS;
      } else if (is_header_part(c)) {
        buffer << c;
        state = READ_HEADER;
      } else {
        return false;
      }
      break;

    case READ_HEADER:
      if (is_header_part(c)) {
        buffer << c;
      } else if (is_colon(c)) {
        current_header_name = buffer.str(); buffer.str("");
        state = START_HEADER_VALUE;
      } else {
        return false;
      }
      break;

    case START_HEADER_VALUE:
      if (is_space(c)) {
        continue;
      } else if (is_header_value_part(c)) {
        buffer << c; state = READ_HEADER_VALUE;
      } else {
        return false;
      }
      break;

    case READ_HEADER_VALUE:
      if (is_header_value_part(c)) {
        buffer << c;
      } else if (is_cr(c)) {
        state = END_HEADER;
      } else {
        return false;
      }
      break;

    case END_HEADER:
      if (is_nl(c)) {
        _headers[current_header_name] = buffer.str(); buffer.str("");
        state = START_HEADER;
      } else {
        return false;
      }
      break;

    case END_HEADERS:
      if (is_nl(c)) { return true; }
      return false;
    }
  }
  return false;
}

bool
Request::isKeepAlive() const {
  if (HTTP_1_1 == _version) { return true; }
  if (HTTP_1_0 == _version) {
    std::map<std::string, std::string>::const_iterator item = _headers.find("Connection");
    if (_headers.end() == item) { return false; }
    if ("Keep-alive" == item->second) { return true; }
  }
  return false;
}

bool
Request::hasHeader(const std::string &name) const {
  return (0 != _headers.count(name));
}

std::string
Request::header(const std::string &name) const {
  std::map<std::string, std::string>::const_iterator item = _headers.find(name);
  return item->second;
}

bool
Request::readBody(std::string &body) const {
  if (! hasContentLength()) { return false; }
  size_t N = contentLength(); body.reserve(N);
  char buffer[65536];
  while (N>0) {
    int res = ::read(_socket, buffer, std::min(N, size_t(65536)));
    if (res>=0) { body.append(buffer, size_t(res)); N -= res; }
    else { return false; }
  }
  return true;
}


/* ********************************************************************************************* *
 * Implementation of HTTPD::Response
 * ********************************************************************************************* */
Response::Response(int socket)
  : _socket(socket), _status(STATUS_SERVER_ERROR), _close_connection(false)
{
  // pass...
}

void
Response::setStatus(Status status) {
  _status = status;
}

bool
Response::hasHeader(const std::string &name) const {
  return (0 != _headers.count(name));
}

std::string
Response::header(const std::string &name) const {
  std::map<std::string, std::string>::const_iterator item = _headers.find(name);
  return item->second;
}

void
Response::setHeader(const std::string &name, const std::string &value) {
  _headers[name] = value;
}

void
Response::setContentLength(size_t length) {
  std::stringstream buffer; buffer << length;
  setHeader("Content-Length", buffer.str());
}

bool
Response::send(const std::string &data) const {
  const char *ptr = data.c_str();
  size_t count = data.size();
  while (count) {
    int c = ::write(_socket, ptr, count);
    if (c < 0) { return false; }
    count -= c; ptr += c;
  }
  return true;
}

bool
Response::sendHeaders() const {
  std::stringstream buffer;
  buffer << "HTTP/1.1 ";
  // Serialize response status
  switch (_status) {
  case STATUS_OK: buffer << "200 OK\r\n"; break;
  case STATUS_BAD_REQUEST: buffer << "400 BAD REQUEST\r\n"; break;
  case STATUS_NOT_FOUND: buffer << "404 NOT FOUND\r\n"; break;
  case STATUS_SERVER_ERROR: buffer << "500 SERVER ERROR\r\n"; break;
  }
  // serialize headers
  std::map<std::string, std::string>::const_iterator item = _headers.begin();
  for (; item != _headers.end(); item++) {
    buffer << item->first << ": " << item->second << "\r\n";
  }
  buffer << "\r\n";
  return send(buffer.str());
}


/* ********************************************************************************************* *
 * Implementation of HTTPD::Handler
 * ********************************************************************************************* */
Handler::Handler()
{
  // pass...
}

Handler::~Handler() {
  // pass...
}


/* ********************************************************************************************* *
 * Implementation of HTTPD::StaticHandler
 * ********************************************************************************************* */
StaticHandler::StaticHandler(const std::string &url, const std::string &text, const std::string mimeType)
  : Handler(), _url(url), _mimeType(mimeType), _text(text)
{
  // pass..
}

StaticHandler::~StaticHandler() {
  // pass...
}

bool
StaticHandler::match(const Request &request) {
  return _url == request.url().path();
}

void
StaticHandler::handle(const Request &request, Response &response) {
  response.setStatus(Response::STATUS_OK);
  if (_mimeType.size()) {
    response.setHeader("Content-type", _mimeType);
  }
  response.setContentLength(_text.size());
  response.sendHeaders();
  response.send(_text);
}


/* ********************************************************************************************* *
 * Implementation of HTTPD::JSONHandler
 * ********************************************************************************************* */
JSONHandler::JSONHandler(const std::string &url)
  : http::Handler(), _url(url)
{
  // pass...
}

bool
JSONHandler::match(const http::Request &request) {
  if (http::HTTP_POST != request.method()) { return false; }
  if (request.url().path() != _url) { return false; }
  if (! request.hasHeader("Content-Type")) { return false; }
  return ("application/json" == request.header("Content-Type"));
}

void
JSONHandler::handle(const http::Request &request, http::Response &response) {
  std::string body;
  if (! request.readBody(body)) {
    response.setStatus(http::Response::STATUS_BAD_REQUEST);
    response.setContentLength(0);
    response.sendHeaders();
  }
  JSON obj;
  if (!JSON::parse(body, obj)) {
    response.setStatus(http::Response::STATUS_BAD_REQUEST);
    response.setContentLength(0);
    response.sendHeaders();
  }

  JSON result;
  std::string result_string;
  if (this->process(obj, result)) {
    response.setStatus(http::Response::STATUS_OK);
    result.serialize(result_string);
    response.setContentLength(result_string.size());
    response.sendHeaders();
    response.send(result_string);
  } else {
    response.setStatus(http::Response::STATUS_BAD_REQUEST);
    response.setContentLength(0);
    response.sendHeaders();
  }
}


/* ********************************************************************************************* *
 * Implementation of JSON
 * ********************************************************************************************* */
JSON::JSON()
  : _type(EMPTY)
{
  // pass..
}

JSON::JSON(bool value)
  : _type(BOOLEAN)
{
  _value.boolean = new bool(value);
}

JSON::JSON(double value)
  : _type(NUMBER)
{
  _value.number = new double(value);
}

JSON::JSON(const std::string &value)
  : _type(STRING)
{
  _value.string = new std::string(value);
}

JSON::JSON(const std::list<JSON> &list)
  : _type(ARRAY)
{
  _value.list = new std::list<JSON>(list);
}

JSON::JSON(const std::map<std::string, JSON> &table)
  : _type(TABLE)
{
  _value.table = new std::map<std::string, JSON>(table);
}

JSON::~JSON() {
  this->clear();
}

JSON::JSON(const JSON &other)
  : _type(other._type)
{
  switch (_type) {
  case EMPTY: break;
  case BOOLEAN: _value.boolean = new bool(other.asBoolean()); break;
  case NUMBER: _value.number = new double(other.asNumber()); break;
  case STRING: _value.string = new std::string(other.asString()); break;
  case ARRAY: _value.list = new std::list<JSON>(other.asArray()); break;
  case TABLE: _value.table = new std::map<std::string, JSON>(other.asTable()); break;
  }
}

JSON &
JSON::operator =(const JSON &other) {
  this->clear();
  _type = other._type;
  switch (_type) {
  case EMPTY: break;
  case BOOLEAN: _value.boolean = new bool(other.asBoolean()); break;
  case NUMBER: _value.number = new double(other.asNumber()); break;
  case STRING: _value.string = new std::string(other.asString()); break;
  case ARRAY: _value.list = new std::list<JSON>(other.asArray()); break;
  case TABLE: _value.table = new std::map<std::string, JSON>(other.asTable()); break;
  }
  return *this;
}

void
JSON::clear() {
  switch (_type) {
  case EMPTY: break;
  case BOOLEAN: delete _value.boolean; break;
  case NUMBER: delete _value.number; break;
  case STRING: delete _value.string; break;
  case ARRAY: delete _value.list; break;
  case TABLE: delete _value.table; break;
  }
  _type = EMPTY;
}

void
JSON::serialize(std::ostream &stream) const {
  switch (_type) {
  case EMPTY:
    stream << "null";
    break;

  case BOOLEAN:
    if (*_value.boolean) { stream << "true"; }
    else { stream << "false"; }
    break;

  case NUMBER:
    stream << *_value.number;
    break;

  case STRING:
    stream << "\"";
    for (size_t i=0; i<_value.string->size(); i++) {
      if ('"' == _value.string->at(i)) { stream << "\\\""; }
      else { stream << _value.string->at(i); }
    }
    stream << "\"";
    break;

  case ARRAY:
    stream << "[";
    if (0 < _value.list->size()) {
      std::list<JSON>::iterator item = _value.list->begin();
      item->serialize(stream); item++;
      for (; item != _value.list->end(); item++) {
        stream << ","; item->serialize(stream);
      }
    }
    stream << "]";
    break;

  case TABLE:
    stream << "{";
    if (0 < _value.table->size()) {
      std::map<std::string, JSON>::iterator item = _value.table->begin();
      stream << item->first << ":"; item->second.serialize(stream); item++;
      for (; item != _value.table->end(); item++) {
        stream << "," << item->first << ":"; item->second.serialize(stream);
      }
    }
    stream << "}";
    break;
  }
}

void
_json_skip_ws(const char *&text, size_t &n) {
  while ((n>0) && is_ws(*text)) { text++; n--; }
}

bool _json_parse(const char *&text, size_t &n, JSON &obj);

  bool
_json_parse_null(const char *&text, size_t &n, JSON &obj) {
  if ((n<4) || (0 != strncmp(text, "null", 4))) { return false; }
  text+=4; n-=4;
  obj = JSON();
  if (0 == n) { return true; }
  if (! is_alpha_num(*text)) { return true; }
  return false;
}

bool
_json_parse_true(const char *&text, size_t &n, JSON &obj) {
  if ((n<4) || (0 != strncmp(text, "true", 4))) { return false; }
  text+=4; n-=4;
  obj = JSON(true);
  if (0 == n) { return true; }
  if (! is_alpha_num(*text)) { return true; }
  return false;
}

bool
_json_parse_false(const char *&text, size_t &n, JSON &obj) {
  if ((n<5) || (0 != strncmp(text, "false", 5))) { return false; }
  text+=5; n-=5;
  if (0 == n) { return true; }
  if (! is_alpha_num(*text)) { return true; }
  obj = JSON(false);
  return false;
}

bool
_json_parse_string(const char *&text, size_t &n, JSON &obj) {
  std::stringstream buffer;
  text++; n--; // skip '"'
  bool escape = false;
  while (n > 0) {
    if (escape) { buffer << *text; escape = false; }
    else if (('\\' == *text) && (!escape)) { escape = true; }
    else if ('"' == *text) { text++; n--; obj = JSON(buffer.str()); return true;}
    else { buffer << *text; }
    text++; n--;
  }
  return false;
}

bool
_json_parse_list(const char *&text, size_t &n, JSON &obj) {
  text++; n--; // skip [
  std::list<JSON> lst;

  _json_skip_ws(text, n);
  if (0 == n) { return false; }
  if (']' == *text) { obj = JSON(lst); text++; n--; return true; }

  JSON tmp;
  while (n>0) {
    if (! _json_parse(text, n, tmp)) { return false; }
    lst.push_back(tmp);
    _json_skip_ws(text, n);
    if (0 == n) { return false; }
    if (']'==*text) { text++; n--; obj = JSON(lst); return true; }
    if (',' != *text) { return false; }
    text++; n--; _json_skip_ws(text, n);
  }
  return false;
}

bool
_json_parse_identifier(const char *&text, size_t &n, std::string &name) {
  if (0 == n) { return false; }
  std::stringstream buffer;
  if (! is_id_start(*text)) { return false; }
  buffer << *text; text++; n--;
  while ((n>0) && is_id_part(*text)) {
    buffer << *text; text++; n--;
  }
  name = buffer.str();
  return true;
}

bool
_json_parse_table(const char *&text, size_t &n, JSON &obj) {
  text++; n--; // skip {
  std::map<std::string, JSON> table;

  _json_skip_ws(text, n);
  if (0 == n) { return false; }
  if ('}' == *text) { obj = JSON(table); text++; n--; return true; }

  std::string name;
  JSON tmp;
  while (n>0) {
    if (! _json_parse_identifier(text, n, name)) { return false; }
    _json_skip_ws(text, n);
    if (0 == n) { return false; }
    if (':' != *text) { return false; }
    text++; n--; _json_skip_ws(text, n);
    if (! _json_parse(text, n, tmp)) { return false; }
    table[name] = tmp;
    _json_skip_ws(text, n);
    if (0 == n) { return false; }
    if (']'==*text) { text++; n--; obj = JSON(table); return true; }
    if (',' != *text) { return false; }
    text++; n--; _json_skip_ws(text, n);
  }
  return false;
}

bool
_json_parse_number(const char *&text, size_t &n, JSON &obj) {
  const char *ptr = text;
  double value = strtof(text, (char **)&ptr);
  if (text == ptr) { return false; }
  obj = JSON(value); text=ptr; n-=(ptr-text);
  return true;
}


bool
_json_parse(const char *&text, size_t &n, JSON &obj) {
  _json_skip_ws(text, n);
  if (0 == n) { return false; }
  // Dispatch by first char
  switch (*text) {
  case 'n': return _json_parse_null(text, n, obj);
  case 't': return _json_parse_true(text, n, obj);
  case 'f': return _json_parse_false(text, n, obj);
  case '"': return _json_parse_string(text, n, obj);
  case '[': return _json_parse_list(text, n, obj);
  case '{': return _json_parse_table(text, n, obj);
  default: return _json_parse_number(text, n, obj);
  }
}

bool
JSON::parse(const std::string &text, JSON &obj) {
  const char *ptr = text.c_str();
  size_t n = text.size();
  return _json_parse(ptr, n, obj);
}
