#ifndef __SDR_QUEUE_HH__
#define __SDR_QUEUE_HH__

#include <list>
#include <map>
#include "buffer.hh"
#include <pthread.h>
#include <iostream>


namespace sdr {

// Forward decl.
class SinkBase;

/** Interface of a delegate. */
class DelegateInterface {
public:
  /** Call back interface. */
  virtual void operator() () = 0;
  virtual void *instance() = 0;
};

/** Specific delegate to a method of an object . */
template <class T>
class Delegate: public DelegateInterface
{
public:
  /** Constructs a delegate to the method @c func of the instance @c instance.*/
  Delegate(T *instance, void (T::*func)(void)) : _instance(instance), _function(func) { }
  /** Destructor. */
  virtual ~Delegate() {}
  /** Callback, simply calls the method of the instance given to the constructor. */
  virtual void operator() () { (_instance->*_function)(); }
  virtual void *instance() { return _instance; }

protected:
  /** The instance. */
  T *_instance;
  /** The method. */
  void (T::*_function)(void);
};


/** Central message queue (singleton). Must be created before any other SDR object is constructed.
 * The queue collects all buffers for processing and routes them to their destination. The queue
 * loop can either be run in a separate thread by passing @c parallel=true to the factory method
 * @c get. In this case, the @c exec method will return immediately. Otherwise, the queue loop
 * will be executed in the thread calling @c exec which blocks until the queue is stopped by
 * a call to @c stop. */
class Queue
{
public:
  /** The internal used message type. */
  class Message {
  public:
    /** Constructor. */
    Message(const RawBuffer &buffer, SinkBase *sink, bool allow_overwrite)
      : _buffer(buffer), _sink(sink), _allow_overwrite(allow_overwrite) { }
    /** Copy constructor. */
    Message(const Message &other)
      : _buffer(other._buffer), _sink(other._sink), _allow_overwrite(other._allow_overwrite) { }
    /** Assignment operator. */
    const Message &operator= (const Message &other) {
      _buffer = other._buffer;
      _sink   = other._sink;
      _allow_overwrite = other._allow_overwrite;
      return *this;
    }
    /** Returns the buffer of the message. **/
    inline const RawBuffer &buffer() const { return _buffer; }
    /** Returns the buffer of the message. **/
    inline RawBuffer &buffer() { return _buffer; }
    /** Returns the destination of the message. **/
    inline SinkBase *sink() const { return _sink; }
    /** If true, the sender allows to overwrite the content of the buffer. **/
    inline bool allowOverwrite() const { return _allow_overwrite; }

  protected:
    /** The buffer being send. */
    RawBuffer _buffer;
    /** The destination. */
    SinkBase *_sink;
    /** If true, the sender allows to overwrite the buffer. */
    bool _allow_overwrite;
  };

protected:
  /** Hidden constructor, use @c get to get the singleton instance. */
  Queue();

public:
  /** Destructor. */
  virtual ~Queue();

  /** Get a reference to the global instance of the queue. If @c parallel is @c true, the
   * queue will be constructed in parallel mode, means the queue loop will be executed in a
   * separate thread. Please note that this option is only used in the first call, when the
   * singleton instance of the queue is created. */
  static Queue &get();

  /** Adds a buffer and its receiver to the queue. If @c allow_overwrite is @c true, the
   * the receiver is allowed to overwrite the content of the buffer. */
  void send(const RawBuffer &buffer, SinkBase *sink, bool allow_overwrite=false);

  /** Enters the queue loop, if @c parallel=true was passed to @c get, @c exec will execute the
   * queue loop in a separate thread and returns immediately. Otherwise, @c exec will block until
   * the queue is stopped. */
  void start();

  /** Signals the queue to stop processing. */
  void stop();
  /** Wait for the queue to exit the queue loop. */
  void wait();

  /** Returns true if the queue loop is stopped. */
  bool isStopped() const;
  /** Returns true if the queue loop is running. */
  bool isRunning() const;

  /** Adds a callback to the idle event. The method gets called repeatedly while the queue looop
   * is idle, means that there are no messages to be processed. This can be used to trigger an
   * input source to read more data. */
  template <class T>
  void addIdle(T *instance, void (T::*function)(void)) {
    _idle.push_back(new Delegate<T>(instance, function));
  }

  template <class T>
  void remIdle(T *instance) {
    std::list<DelegateInterface *>::iterator item = _idle.begin();
    while (item != _idle.end()) {
      if ( (*item)->instance() == ((void *)instance)) {
        item = _idle.erase(item);
      } else {
        item++;
      }
    }
  }

  /** Adds a callback to the start event. The method gets called once the queue loop is started. */
  template <class T>
  void addStart(T *instance, void (T::*function)(void)) {
    _onStart.push_back(new Delegate<T>(instance, function));
  }

  template <class T>
  void remStart(T *instance) {
    std::list<DelegateInterface *>::iterator item = _onStart.begin();
    while (item != _onStart.end()) {
      if ( (*item)->instance() == ((void *)instance)) {
        item = _onStart.erase(item);
      } else {
        item++;
      }
    }
  }

  /** Adds a callback to the stop event. The method gets called once the queue loop is stopped. */
  template <class T>
  void addStop(T *instance, void (T::*function)(void)) {
    _onStop.push_back(new Delegate<T>(instance, function));
  }

  template <class T>
  void remStop(T *instance) {
    std::list<DelegateInterface *>::iterator item = _onStop.begin();
    while (item != _onStop.end()) {
      if ( (*item)->instance() == ((void *)instance)) {
        item = _onStop.erase(item);
      } else {
        item++;
      }
    }
  }

protected:
  /** The actual queue loop. */
  void _main();
  /** Emits the idle signal. */
  void _signalIdle();
  /** Emits the start signal. */
  void _signalStart();
  /** Emits the stop signal. */
  void _signalStop();

protected:
  /** While this is true, the queue loop is executed. */
  bool _running;
  /** If @c _parallel is true, the thread of the queue loop. */
  pthread_t _thread;
  /** The queue mutex. */
  pthread_mutex_t _queue_lock;
  /** The queue condition. */
  pthread_cond_t  _queue_cond;

  /** The message queue. */
  std::list<Message> _queue;
  /** Idle event callbacks. */
  std::list<DelegateInterface *> _idle;
  /** Start event callbacks. */
  std::list<DelegateInterface *> _onStart;
  /** Stop event callbacks. */
  std::list<DelegateInterface *> _onStop;

private:
  /** The singleton instance. */
  static Queue *_instance;
  /** The pthread function. */
  static void *__thread_start(void *ptr);
};


}
#endif // __SDR_QUEUE_HH__
