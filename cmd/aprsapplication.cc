#include "aprsapplication.hh"

// This file will be generated at build time
// and contains the static content served.
#include "sdr_cmd_resources.hh"


using namespace sdr;

APRSApplication::APRSApplication(http::Server &server)
  : APRS(), _server(server), _messages()
{
  // Register callbacks
  server.addStatic("/", std::string(index_html, index_html_size), "text/html");
  server.addJSON("/spots", this, &APRSApplication::spots);
  server.addHandler("/update", this, &APRSApplication::update);
}

APRSApplication::~APRSApplication() {
  // pass...
}


bool
APRSApplication::spots(const http::JSON &request, http::JSON &response) {
  std::list<http::JSON> msg_list;
  for (std::list<Message>::iterator msg = _messages.begin(); msg != _messages.end(); msg++) {
    std::map<std::string, http::JSON> message;
    message["call"] = http::JSON(msg->from().call());
    if (msg->hasLocation()) {
      message["lat"] = http::JSON(msg->latitude());
      message["lon"] = http::JSON(msg->longitude());
    }
    time_t time = msg->time();
    message["time"] = http::JSON(ctime(&time));
    msg_list.push_back(message);
  }
  response = http::JSON(msg_list);
  return true;
}

void
APRSApplication::handleAPRSMessage(const Message &message) {
  _messages.push_back(message);
  // Serialize JSON message
  std::string json_text;
  if (_clients.size()) {
    std::map<std::string, http::JSON> msg;
    msg["call"] = http::JSON(message.from().call());
    if (message.hasLocation()) {
      msg["lat"] = http::JSON(message.latitude());
      msg["lon"] = http::JSON(message.longitude());
    }
    time_t time = message.time();
    msg["time"] = http::JSON(ctime(&time));
    http::JSON(msg).serialize(json_text);
  }
  // a list collecting the closed
  // signal all clients connected
  std::list<http::Connection>::iterator client = _clients.begin();
  while (client != _clients.end()) {
    if (client->isClosed()) {
      // remove client from list
      client = _clients.erase(client);
    } else {
      // send event
      client->send("data: ");
      client->send(json_text);
      client->send("\n\n");
    }
  }
}

void
APRSApplication::update(const http::Request &request, http::Response &response) {
  // This call back implements a server side event stream, means the response will
  // be blocked until the connection is closed
  response.setHeader("Content-Type", "text/event-stream");
  response.setHeader("Cache-Control", "no-cache");
  response.setStatus(http::Response::STATUS_OK);
  response.sendHeaders();
  // Signal connection thread to exit without closing the connection
  response.connection().setProtocolUpgrade();
  // Store connection
  _clients.push_back(response.connection());
}

