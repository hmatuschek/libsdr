/** @defgroup httpd A rather trivia HTTP daemon implementation. */

#ifndef __SDR_HTTPD_HH__
#define __SDR_HTTPD_HH__

#include <string>
#include <sstream>
#include <map>
#include <set>
#include <list>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>


namespace sdr {
namespace http {

// Forward declarations
class Server;


/** Represents a JSON object.
 * @ingroup http */
class JSON
{
public:
  /** Specifies the possible json types. */
  typedef enum {
    EMPTY,     ///< null.
    BOOLEAN,   ///< A boolean value.
    NUMBER,    ///< A number.
    STRING,    ///< A string.
    ARRAY,     ///< An array or list.
    TABLE      ///< A table or object.
  } Type;

public:
  /** Empty constructor (null). */
  JSON();
  /** Constructs a boolean value. */
  JSON(bool value);
  /** Constructs a number. */
  JSON(double value);
  /** Constructs a string. */
  JSON(const std::string &value);
  /** Constructs a list. */
  JSON(const std::list<JSON> &list);
  /** Constructs a table. */
  JSON(const std::map<std::string, JSON> &table);
  /** Copy constructor (deep copy). */
  JSON(const JSON &other);
  /** Destructor. */
  ~JSON();
  /** Assignment operator (deep copy). */
  JSON &operator=(const JSON &other);

  /** Retruns @c true for the null. */
  inline bool isNull() const { return EMPTY == _type; }
  /** Returns @c true if this is a boolean value. */
  inline bool isBoolean() const { return BOOLEAN == _type; }
  /** Returns the boolean value. */
  inline const bool &asBoolean() const { return *_value.boolean; }
  /** Returns @c true if this is a number. */
  inline bool isNumber() const { return NUMBER == _type; }
  /** Returns the number. */
  inline const double &asNumber() const { return *_value.number; }
  /** Returns @c true if this is a string. */
  inline bool isString() const { return STRING == _type; }
  /** Returns the string. */
  inline const std::string &asString() const { return *_value.string; }
  /** Returns @c true if this is an array. */
  inline bool isArray() const { return ARRAY == _type; }
  /** Retruns the array as a list. */
  inline const std::list<JSON> &asArray() const { return *_value.list; }
  /** Returns @c true if this is a table. */
  inline bool isTable() const { return TABLE == _type; }
  /** Returns the table. */
  inline const std::map<std::string, JSON> &asTable() const { return *_value.table; }
  /** Resets the JSON object to null. */
  void clear();

  /** Parses the given string. Retruns @c true on success. */
  static bool parse(const std::string &text, JSON &obj);
  /** Serializes the JSON object. */
  void serialize(std::ostream &stream) const;
  /** Serializes the JSON object. */
  inline void serialize(std::string &text) const {
    std::stringstream buffer; this->serialize(buffer);
    text = buffer.str();
  }

protected:
  /** The type of the object. */
  Type _type;
  /** Union of value pointers. */
  union {
    /** The boolean value. */
    bool   *boolean;
    /** The number. */
    double *number;
    /** The string. */
    std::string *string;
    /** The array. */
    std::list<JSON> *list;
    /** The table. */
    std::map<std::string, JSON> *table;
  } _value;
};


/** Lists the possible HTTP methods.
 * @ingroup http */
typedef enum {
  HTTP_UNKNOWN,
  HTTP_GET,
  HTTP_HEAD,
  HTTP_POST
} Method;

/** Lists the possible HTTP versions.
 * @ingroup http */
typedef enum {
  UNKNOWN_VERSION,
  HTTP_1_0,
  HTTP_1_1
} Version;

/** Represents a URL.
 * @ingroup http */
class URL
{
public:
  /** Empty constructor. */
  URL();
  /** Constructor from protocol, host and path. */
  URL(const std::string &proto, const std::string &host, const std::string &path);
  /** Copy constructor. */
  URL(const URL &other);

  /** Assignment operator. */
  URL &operator=(const URL &other);

  /** Parses a URL from the given string. */
  static URL fromString(const std::string &url);
  /** Serializes the URL into a string. */
  std::string toString() const;

  /** Returns @c true if the URL specifies a protocol. */
  inline bool hasProtocol() const { return (0 != _protocol.size()); }
  /** Returns the protocol. */
  inline const std::string protocol() const { return _protocol; }
  /** Sets the protocol. */
  inline void setProtocol(const std::string &proto) { _protocol = proto; }
  /** Returns @c true if the URL specified a host name. */
  inline bool hasHost() const { return (0 != _host.size()); }
  /** Returns the host name. */
  inline const std::string &host() const { return _host; }
  /** Set the host name. */
  inline void setHost(const std::string &host) { _host = host; }
  /** Retruns the path of the URL. */
  inline const std::string &path() const { return _path; }
  /** Sets the path of the URL. */
  inline void setPath(const std::string &path) { _path = path; }
  /** Adds a query pair (key, value) to the URL. */
  inline void addQuery(const std::string &name, const std::string &value) {
    _query.push_back(std::pair<std::string, std::string>(name, value));
  }
  /** Returns the list of query (key, value) pairs. */
  inline const std::list< std::pair<std::string, std::string> > &query() const {
    return _query;
  }

protected:
  /** Holds the protocol. */
  std::string _protocol;
  /** Holds the host name. */
  std::string _host;
  /** Holds the path. */
  std::string _path;
  /** Holds the query pairs. */
  std::list< std::pair<std::string, std::string> > _query;
};


/** Represents a HTTP request.
 * @ingroup http */
class Request
{
public:
  /** Constructor. */
  Request(int socket);

  /** Parses the HTTP request header, returns @c true on success. */
  bool parse();

  /** Returns @c true if the connection to the client is kept alive after the response
     * has been send. */
  bool isKeepAlive() const;

  /** Returns @c true if the given header is present. */
  bool hasHeader(const std::string &name) const;
  /** Returns the value of the given header. */
  std::string header(const std::string &name) const;

  /** Returns @c true if the Content-Length header is present. */
  inline bool hasContentLength() const { return hasHeader("Content-Length"); }
  /** Returns the value of the Content-Length header. */
  size_t contentLength() const { return atol(header("Content-Length").c_str()); }

  /** Retruns the request method. */
  inline Method method() const { return _method; }
  /** Returns the request URL. */
  inline const URL &url() const { return _url; }
  /** Allows to read data from the connection (i.e. the request body). */
  inline int read(const char *data, size_t size) {
    return ::read(_socket, (void *)data, size);
  }
  /** Reads the complete body (if Content-Length header is present).
     * Retruns @c true on success.*/
  bool readBody(std::string &body) const;

protected:
  /** The connection socket. */
  int _socket;
  /** The request method. */
  Method _method;
  /** The HTTP version. */
  Version _version;
  /** The request URL. */
  URL _url;
  /** The request headers. */
  std::map<std::string, std::string> _headers;
};


/** Represents a HTTP response. */
class Response
{
public:
  /** Defines all possible responses. */
  typedef enum {
    STATUS_OK = 200,
    STATUS_BAD_REQUEST = 400,
    STATUS_NOT_FOUND = 404,
    STATUS_SERVER_ERROR = 500
  } Status;

public:
  /** Constructor.
   * @param socket Specifies the socket over which the response will be send.*/
  Response(int socket);

  /** Specifies the response code. */
  void setStatus(Status status);
  /** Returns @c true if the response has the given header. */
  bool hasHeader(const std::string &name) const;
  /** Returns the value of the header. */
  std::string header(const std::string &name) const;
  /** Sets the header. */
  void setHeader(const std::string &name, const std::string &value);
  /** Helper function to set the content length. */
  void setContentLength(size_t length);

  /** Sends the response code and all defined headers. */
  bool sendHeaders() const;

  /** Sends some data through the socket. To send the response body, call
   * @c sendHeaders() first. */
  bool send(const std::string &data) const;

  /** Returns @c true if the connection will be closed after the response has been send.
   * @c I.e. if there was an error. */
  inline bool closeConnection() const { return _close_connection; }
  /** Marks that the connection will be closed after the response has been send. */
  inline void setCloseConnection() { _close_connection = true; }

protected:
  /** The socket over which the response will be send. */
  int _socket;
  /** The response code. */
  Status _status;
  /** The response headers. */
  std::map<std::string, std::string> _headers;
  /** If @c true, the connection will be closed after the response has been send. */
  bool _close_connection;
};


/** Implements a HTTP connection to a client.
 * @c ingroup http */
class Connection
{
public:
  /** Constructor. */
  Connection(Server *server, int socket);
  /** Destructor. */
  ~Connection();

  /** Closes the connection.
   * If @c wait is @c true, the method will wait until the thread listening for incomming
   * request joined. */
  void close(bool wait=false);
  /** Returns @c true if the connection is closed. */
  bool isClosed() const;

protected:
  /** Main loop for incomming requests. */
  static void *_main(void *ctx);

protected:
  /** A weak reference to the server instance. */
  Server *_server;
  /** The connection socket. */
  int _socket;
  /** The thread processing requests for this connection. */
  pthread_t _thread;
};


/** Base class of all HTTP request handlers.
 * @ingroup http */
class Handler
{
protected:
  /** Hidden constructor. */
  Handler();

public:
  /** Destructor. */
  virtual ~Handler();
  /** Needs to be implemented to accept the given request.
   * If this method returns @c true, the server will call the @c handle instance next
   * in expectation that this instance will process the request. */
  virtual bool match(const Request &request) = 0;
  /** Needs to be implemented to process requests, which have been accepted by the @c match()
   * method. */
  virtual void handle(const Request &request, Response &response) = 0;
};


/** Serves some static content.
 * @ingroup http */
class StaticHandler: public Handler
{
public:
  /** Constructor.
   * @param url Specifies the URL (path) of the static content.
   * @param text Speciefies the content.
   * @param mimeType Speficies the mime-type of the content. */
  StaticHandler(const std::string &url, const std::string &text,
                const std::string mimeType="text/text");
  /** Destructor. */
  virtual ~StaticHandler();

  bool match(const Request &request);
  void handle(const Request &request, Response &response);

protected:
  /** Holds the URL (path) of the content. */
  std::string _url;
  /** Holds the mime-type of the content. */
  std::string _mimeType;
  /** Holds the content itself. */
  std::string _text;
};


/** Utility function to provide a handler as a delegate.
 * @ingroup http */
template <class T>
class DelegateHandler: public Handler
{
public:
  /** Constructor.
   * @param url Specifies the path of the handler.
   * @param instance Specifies the instance of the delegate.
   * @param func Specifies the method of the delegate instance being called. */
  DelegateHandler(const std::string &url, T *instance, void (T::*func)(const Request &, Response &))
    : Handler(), _url(url), _instance(instance), _callback(func)
  {
    // pass...
  }

  bool match(const Request &request) {
    return this->_url == request.url().path();
  }

  void handle(const Request &request, Response &response) {
    (this->_instance->*(this->_callback))(request, response);
  }

protected:
  /** Holds the path of the handler. */
  std::string _url;
  /** Holds the instance of the delegate. */
  T *_instance;
  /** Holds the method of the instance being called. */
  void (T::*_callback)(const Request &, Response &);
};


/** Implements a specialized handler to ease the processing of JSON REST-APIs.
 * @ingroup http */
class JSONHandler: public http::Handler
{
public:
  /** Constructor.
   * @param url Speficies the path of the method. */
  JSONHandler(const std::string &url);

  bool match(const http::Request &request);
  void handle(const http::Request &request, http::Response &response);

  /** Needs to be implemented to process the request and assemble the result.
   * An error (400 BAD REQUEST) will be send if the function does not return @c true. */
  virtual bool process(const JSON &request, JSON &result) = 0;

protected:
  /** The URL of the method. */
  std::string _url;
};


/** A delegate JSON handler.
 * @ingroup http */
template <class T>
class DelegateJSONHandler: public JSONHandler
{
public:
  /** Constructor. */
  DelegateJSONHandler(const std::string &url, T *instance,
                      bool (T::*func)(const JSON &request, JSON &response))
    : JSONHandler(url), _instance(instance), _callback(func)
  {
    // pass...
  }

  virtual bool process(const JSON &request, JSON &result) {
    return (this->_instance->*(this->_callback))(request, result);
  }

protected:
  /** The delegate instance. */
  T *_instance;
  /** The method to be called from the delegate instance. */
  bool (T::*_callback)(const JSON &request, JSON &response);
};


/** Implements a trivial HTTP/1.1 server.
 * @ingroup httpd*/
class Server
{
public:
  /** Constructor.
   * @param port Specifies the port number to listen on. */
  Server(uint port);
  /** Destructor. */
  ~Server();

  /** Starts the server. */
  void start(bool wait=false);
  /** Stops the server. */
  void stop(bool wait=false);
  /** Wait for the server thread to join. */
  void wait();

  /** Adds a generic handler to the dispatcher. */
  void addHandler(Handler *handler);
  /** Adds a delegate to the dispatcher. */
  template <class T>
  void addHandler(const std::string &url,
                  T *instance, void (T::*func)(const Request &, Response &)) {
    addHandler(new DelegateHandler<T>(url, instance, func));
  }
  /** Adds a JSON delegate to the dispatcher. */
  template <class T>
  void addJSON(const std::string &url,
               T *instance, bool (T::*func)(const JSON &request, JSON &result)) {
    addHandler(new DelegateJSONHandler<T>(url, instance, func));
  }
  /** Adds some static content to the dispatcher. */
  inline void addStatic(const std::string &url, const std::string &text) {
    addHandler(new StaticHandler(url, text));
  }
  /** Adds a generic handler to the dispatcher. */
  inline void addStatic(const std::string &url, const std::string &text, const std::string &mimeType) {
    addHandler(new StaticHandler(url, text, mimeType));
  }

protected:
  /** Dispatches a request. */
  void dispatch(const Request &request, Response &response);
  /** The thread waiting for incomming connections. */
  static void *_listen_main(void *ctx);

protected:
  /** Port to bind to. */
  uint _port;
  /** The socket to listen on. */
  int _socket;
  /** While true, the server is listening on the port for incomming connections. */
  bool _is_running;
  /** The listen thread. */
  pthread_t _thread;
  /** All registered handler. */
  std::list<Handler *> _handler;
  /** All open connections. */
  std::set<Connection *> _connections;

  /* Allow Connection to access dispatch(). */
  friend class Connection;
};

}

}
#endif // __SDR_HTTPD_HH__
