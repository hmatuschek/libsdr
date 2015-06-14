#include <stdio.h>

#include "queue.hh"
#include "logger.hh"

#include <iostream>
#include <csignal>

#include "aprsapplication.hh"

using namespace sdr;


static http::Server *server = 0;

static void __sigint_handler(int signo) {
  if (server) { server->stop(true); }
}


int main(int argc, char *argv[]) {
  server = new http::Server(8080);
  APRSApplication app(*server);

  // Install log handler
  sdr::Logger::get().addHandler(
        new sdr::StreamLogHandler(std::cerr, sdr::LOG_DEBUG));

  // Register signal handler:
  signal(SIGINT, __sigint_handler);

  // start server
  server->start(true);

  return 0;
}
