#include "ScopeTimer.h"

thread_local int ScopeTimer::depth = 0;
thread_local std::queue<ScopeTimer::__> ScopeTimer::stack_;

ScopeTimer::ScopeTimer(const std::string &tag) : tag_(tag) {
  last_ = std::chrono::high_resolution_clock::now();
  ++depth;
}

static void print(double cost, const std::string &tag, int depth) {
  for (int d = 0; d < depth; d++) {
    std::cout << "  ";
  }
  std::cout << "[" << tag << "]: costs: " << cost << " seconds" << std::endl;
}

ScopeTimer::~ScopeTimer() {
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::high_resolution_clock::now() - last_);
  double seconds = double(duration.count()) *
                   std::chrono::microseconds::period::num /
                   std::chrono::microseconds::period::den;
  --depth;

  //if (depth == 0) { // output all
  //  if (!stack_.empty()) {
  //    while (!stack_.empty()) {
  //      auto t = stack_.front();
  //      print(t.cost, t.tag, t.depth);
  //      stack_.pop();
  //    }
  //  } else {
      print(seconds, tag_, depth);
  /*  }
  } else {
    stack_.push({seconds, depth, move(tag_)});
  }*/
}