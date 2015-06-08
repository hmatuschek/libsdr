#include <stdio.h>

#include "queue.hh"
#include "http.hh"
#include "logger.hh"

#include <iostream>
#include <csignal>
#include "sdr_cmd_resources.hh"

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

int main(int argc, char *argv[]) {
  Application app;
  server = new http::Server(8080);

  // Install log handler
  sdr::Logger::get().addHandler(
        new sdr::StreamLogHandler(std::cerr, sdr::LOG_DEBUG));

  // Register signal handler:
  signal(SIGINT, __sigint_handler);

  // Register callbacks
  server->addStatic("/", std::string(index_html, index_html_size), "text/html");
  server->addJSON("/echo", &app, &Application::echo);
  // start server
  server->start(true);

  return 0;
}
