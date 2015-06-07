#include <stdio.h>

#include "queue.hh"
#include "http.hh"
#include "logger.hh"

#include <iostream>
#include <csignal>

using namespace sdr;

class Application
{
public:
  Application() {}

  bool echo(const http::JSON &request, http::JSON &response) {
    response = request;
    return true;
  }
};


static http::Server *server = 0;

static void __sigint_handler(int signo) {
  server->stop(true);
}

const char *index_html = "<html>"
    "<head></head>"
    "<body>"
    "<b>It is alive!</b>"
    "<body>"
    "</html>";


bool
json_echo(const http::JSON &request, http::JSON &response) {
  response = request;
  return true;
}


int main(int argc, char *argv[]) {
  Application app;
  server = new http::Server(8080);

  // Install log handler
  sdr::Logger::get().addHandler(
        new sdr::StreamLogHandler(std::cerr, sdr::LOG_DEBUG));

  // Register signal handler:
  signal(SIGINT, __sigint_handler);

  // Register callbacks
  server->addStatic("/", index_html, "text/html");
  server->addJSON("/echo", &app, &Application::echo);

  // start server
  server->start(true);

  return 0;
}
