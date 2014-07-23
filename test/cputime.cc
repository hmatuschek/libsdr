#include "cputime.hh"

using namespace UnitTest;


CpuTime::CpuTime()
{
}


void
CpuTime::start()
{
  this->_clocks.push_back(clock());
}


double
CpuTime::stop()
{
  // measure time.
  clock_t end = clock();

  // Get time-diff since start:
  double dt = end-this->_clocks.back();
  dt /= CLOCKS_PER_SEC;

  // Remove start time from stack:
  this->_clocks.pop_back();

  // Return delta t:
  return dt;
}


double
CpuTime::getTime()
{
  clock_t end = clock();

  // get diff:
  double dt = end - this->_clocks.back();
  dt /= CLOCKS_PER_SEC;

  return dt;
}
