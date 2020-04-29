#ifndef __SDR_UNITTEST_HH__
#define __SDR_UNITTEST_HH__

#include <list>
#include <string>
#include <sstream>
#include <cmath>


namespace UnitTest {

class TestFailure : public std::exception
{
protected:
  std::string message;

public:
  TestFailure(const std::string &message) throw();
  virtual ~TestFailure() throw();

  const char *what() const throw();
};



class TestCase
{
public:
  virtual void setUp();
  virtual void tearDown();

  void assertTrue(bool test, const std::string &file, size_t line);

  template <class Scalar>
  void assertEqual(Scalar t, Scalar e, const std::string &file, size_t line) {
    if (e != t) {
      std::stringstream str;
      str << "Expected: " << +e << " but got: " << +t
          << " in file "<< file << " in line " << line;
      throw TestFailure(str.str());
    }
  }

  template <class Scalar>
  void assertNear(Scalar t, Scalar e, const std::string &file, size_t line,
                  Scalar err_abs=Scalar(1e-8), Scalar err_rel=Scalar(1e-6))
  {
    if (std::abs(e-t) > (err_abs + err_rel*std::abs(e))) {
      std::stringstream str;
      str << "Expected: " << +e << " but got: " << +t
          << " in file "<< file << " in line " << line;
      throw TestFailure(str.str());
    }
  }
};



class TestCallerInterface
{
protected:
  std::string description;

public:
  TestCallerInterface(const std::string &desc)
    : description(desc)
  {
    // Pass...
  }

  virtual ~TestCallerInterface() { /* pass... */ }

  virtual const std::string &getDescription()
  {
    return this->description;
  }

  virtual void operator() () = 0;
};


template <class T>
class TestCaller : public TestCallerInterface
{
protected:
  void (T::*function)(void);

public:
  TestCaller(const std::string &desc, void (T::*func)(void))
    : TestCallerInterface(desc), function(func)
  {
    // Pass...
  }

  virtual ~TestCaller() { /* pass... */ }

  virtual void operator() ()
  {
    // Create new test:
    T *instance = new T();

    // Call test
    instance->setUp();
    (instance->*function)();
    instance->tearDown();

    // free instance:
    delete instance;
  }
};


class TestSuite
{
public:
  typedef std::list<TestCallerInterface *>::iterator iterator;

protected:
  std::string description;
  std::list<TestCallerInterface *> tests;

public:
  TestSuite(const std::string &desc);
  virtual ~TestSuite();

  void addTest(TestCallerInterface *test);

  const std::string &getDescription();

  iterator begin();
  iterator end();
};


class TestRunner
{
protected:
  std::ostream &stream;
  std::list<TestSuite *> suites;

public:
  TestRunner(std::ostream &stream);
  virtual ~TestRunner();

  void addSuite(TestSuite *suite);

  size_t operator() ();
};


#define UT_ASSERT(t) this->assertTrue(t, __FILE__, __LINE__)
#define UT_ASSERT_EQUAL(t, e) this->assertEqual(t, e, __FILE__, __LINE__)
#define UT_ASSERT_NEAR(t, e) this->assertNear(t, e, __FILE__, __LINE__)
#define UT_ASSERT_THROW(t, e) \
  try { t; throw UnitTest::TestFailure("No exception thrown!"); } catch (e &err) {}
}


#endif // UNITTEST_HH
