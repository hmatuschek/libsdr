#ifndef __SDR_APRS_APRSAPPLICATION_HH__
#define __SDR_APRS_APRSAPPLICATION_HH__

#include "http.hh"
#include "aprs.hh"


namespace sdr {

class APRSApplication: public APRS
{
public:
  APRSApplication(http::Server &server);
  ~APRSApplication();

  bool spots(const http::JSON &request, http::JSON &response);
  void update(const http::Request &request, http::Response &response);

  void handleAPRSMessage(const Message &message);

protected:
  http::Server      &_server;
  std::list<Message> _messages;
  std::list<http::Connection> _clients;
};

}

#endif // APRSAPPLICATION_HH
