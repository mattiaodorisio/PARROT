#pragma once

#define check(condition) \
do { \
  if (!(condition)) { \
    std::cerr << "Check failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
    std::abort(); \
  } \
} while (0)
