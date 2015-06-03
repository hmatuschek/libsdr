#ifndef __SDR_APRS_HH__
#define __SDR_APRS_HH__

#include "ax25.hh"
#include <ctime>


namespace sdr {

class APRS: public AX25
{
public:
  class Message: public AX25::Message
  {
  public:
    /** A small selection of possible symbols to display the station. */
    typedef enum {
      NONE, POLICE, DIGI, PHONE, AIRCRAFT, HOUSE, MOTORCYCLE, CAR, BBS, BALLOON, BUS,
      BOAT, JOGGER, WX
    } Symbol;

  public:
    Message();
    Message(const AX25::Message &msg);
    Message(const Message &msg);

    Message &operator=(const Message &other);

    inline bool hasLocation() const { return _hasLocation; }
    inline double latitude() const { return _latitude; }
    inline double longitude() const { return _longitude; }
    inline Symbol symbol() const { return _symbol; }

    inline const time_t &time() const { return _time; }

    inline bool hasComment() const { return (0 != _comment.size()); }
    inline const std::string &comment() const { return _comment; }

  protected:
    bool _readLocation(size_t &offset);
    bool _readLatitude(size_t &offset);
    bool _readLongitude(size_t &offset);
    bool _readTime(size_t &offset);

  protected:
    bool _hasLocation;
    double _latitude;
    double _longitude;
    Symbol _symbol;
    bool _hasTime;
    time_t _time;
    std::string _comment;
  };

public:
  APRS();

  void handleAX25Message(const AX25::Message &message);
  virtual void handleAPRSMessage(const Message &message);
};

std::ostream& operator<<(std::ostream &stream, const APRS::Message &msg);

}

#endif // __SDR_APRS_HH__
