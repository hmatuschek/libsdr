#ifndef __SDR_CPUTIME_HH__
#define __SDR_CPUTIME_HH__

#include <time.h>
#include <list>


namespace UnitTest {

/** A utility class to measure the CPU time used by some algorithms. */
class CpuTime
{
public:
  /** Constructs a new CPU time clock. */
  CpuTime();

  /** Start the clock.  */
  void start();
  /** Stops the clock and returns the time in seconds. */
  double stop();
  /**  Retruns the current time of the current clock. */
  double getTime();

protected:
  /** The stack of start times. */
  std::list< clock_t > _clocks;
};

}

#endif // CPUTIME_HH
