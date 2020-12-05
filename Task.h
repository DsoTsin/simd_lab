#pragma once

#include "CoreMinimal.h"

namespace simd {
class ITask {
public:
  ITask() : finished_(0), event_(new Event(false)) {}
  virtual ~ITask() { delete event_; }

  virtual void run() {}

  void wait() { event_->wait(); }

  bool isComplete() const { return finished_ == 1; }

private:
  void finish() {
    event_->trigger();
    finished_ = 1;
  }

  friend class TaskThread;

  volatile int32 finished_;
  Event *event_;
};

class TaskThread {
public:
  TaskThread();
  ~TaskThread();

  void launch();

  void enqueue(ITask *task);

private:
  Thread *thread_;
  volatile int stopped_;
  moodycamel::ConcurrentQueue<ITask *> taskQueue_;
};

} // namespace simd