#include "queue.hh"
#include "node.hh"
#include "config.hh"
#include "logger.hh"

using namespace sdr;


/* ********************************************************************************************* *
 * Implementation of Queue class
 * ********************************************************************************************* */
Queue *Queue::_instance = 0;

Queue &
Queue::get() {
  if (0 == Queue::_instance) {
    Queue::_instance = new Queue();
  }
  return *Queue::_instance;
}

Queue::Queue()
  : _running(false)
{
  // allocate queue lock and condition
  pthread_mutex_init(&_queue_lock, NULL);
  pthread_cond_init(&_queue_cond, NULL);
}

Queue::~Queue() {
  pthread_mutex_destroy(&_queue_lock);
  pthread_cond_destroy(&_queue_cond);
}

void
Queue::send(const RawBuffer &buffer, SinkBase *sink, bool allow_overwrite) {
  // Refrerence buffer
  pthread_mutex_lock(&_queue_lock);
  buffer.ref();
  _queue.push_back(Message(buffer, sink, allow_overwrite));
  pthread_cond_signal(&_queue_cond);
  pthread_mutex_unlock(&_queue_lock);
}

bool
Queue::isStopped() const {
  return !_running;
}

bool
Queue::isRunning() const {
  return _running;
}


void
Queue::start() {
  if (_running) { return; }
  pthread_create(&_thread, 0, Queue::__thread_start, this);
}


void
Queue::stop() {
  _running = false;
  pthread_cond_signal(&_queue_cond);
}

void
Queue::wait() {
  // Wait for the queue to quit
  void *p; pthread_join(_thread, &p);

  // Clear queue.
  std::list<Message>::iterator item = _queue.begin();
  for (; item != _queue.end(); item++) {
    item->buffer().unref();
  }
  _queue.clear();
}


void
Queue::_main()
{
  // set state
  _running = true;

  Logger::get().log(LogMessage(LOG_DEBUG, "Queue started."));

  // Call all start signal handlers...
  _signalStart();

  // As long as the queue runs or there are any buffers left to be processed
  while (_running || (_queue.size() > 0)) {
    // Process all messages in queue
    while (_queue.size() > 0) {
      // Get a Message from the queue
      pthread_mutex_lock(&_queue_lock);
      Message msg(_queue.front()); _queue.pop_front();
      pthread_mutex_unlock(&_queue_lock);
      // Process message
      msg.sink()->handleBuffer(msg.buffer(), msg.allowOverwrite());
      // Mark buffer unused
      msg.buffer().unref();
    }

    // If there are no buffer in the queue and the queue is still running:
    if ((0 == _queue.size()) && _running) {
      // Signal idle handlers
      _signalIdle();
      //  -> wait until a buffer gets available
      pthread_mutex_lock(&_queue_lock);
      while( (0 == _queue.size()) && _running ) { pthread_cond_wait(&_queue_cond, &_queue_lock); }
      pthread_mutex_unlock(&_queue_lock);
    }
  }
  // Call all stop-signal handlers
  _signalStop();
  {
    LogMessage msg(LOG_DEBUG, "Queue stopped.");
    msg << " Messages left in queue: " << _queue.size();
    Logger::get().log(msg);
  }
}

void
Queue::_signalIdle() {
  std::list<DelegateInterface *>::iterator item = _idle.begin();
  for (; item != _idle.end(); item++) {
    (**item)();
  }
}

void
Queue::_signalStart() {
  std::list<DelegateInterface *>::iterator item = _onStart.begin();
  for (; item != _onStart.end(); item++) {
    (**item)();
  }
}

void
Queue::_signalStop() {
  std::list<DelegateInterface *>::iterator item = _onStop.begin();
  for (; item != _onStop.end(); item++) {
    (**item)();
  }
}

void *
Queue::__thread_start(void *ptr) {
  Queue *queue = reinterpret_cast<Queue *>(ptr);
  try {
    queue->_main();
  } catch (std::exception &err) {
    LogMessage msg(LOG_ERROR);
    msg << "Caught exception in thread: " << err.what()
        << " -> Stop thread.";
    Logger::get().log(msg);
  } catch (...) {
    LogMessage msg(LOG_ERROR);
    msg << "Caught (known) exception in thread -> Stop thread.";
    Logger::get().log(msg);
  }
  queue->_running = false;
  pthread_exit(0);
  return 0;
}
