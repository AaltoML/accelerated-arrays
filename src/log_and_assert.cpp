#include "assert.hpp"
#include "log.hpp"

#include <cstdlib>

namespace accelerated {
void assert_fail(const char *assertion, const char *fn, unsigned int line, const char *func) {
  log_error("assertion %s failed in %s (%s:%u)", assertion, func, fn, line);
  std::abort();
}
}
