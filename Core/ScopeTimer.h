#pragma once
#include "CoreMinimal.h"

class ScopeTimer {
public:
  explicit ScopeTimer(const std::string &tag);
  ~ScopeTimer();

private:
	struct __ {
	double cost;
	int depth;
	std::string tag;
  };

  std::chrono::high_resolution_clock::time_point last_;
  std::string tag_;

  static thread_local std::queue<__> stack_;
  static thread_local int depth;
};