#include "Task.h"

namespace simd {
TaskThread::TaskThread() : thread_(nullptr), stopped_(0) {
  thread_ = new Thread(
      [this]() {
        while (stopped_ == 0) {
          while (taskQueue_.size_approx() > 0) {
            ITask *task = nullptr;
            if (taskQueue_.try_dequeue(task)) {
              task->run();
              task->finish();
            }
          }
          Thread::SleepForMilliSeconds(1);
        }
      },
      "test");
}
TaskThread::~TaskThread() {
  if (thread_) {
    stopped_ = 1;
    thread_->Join();
    delete thread_;
    thread_ = nullptr;
  }
}
void TaskThread::launch() {
  check(thread_);
  thread_->Start();
}
void TaskThread::enqueue(ITask *task) { taskQueue_.enqueue(task); }
} // namespace simd