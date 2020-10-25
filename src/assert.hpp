#pragma once

namespace accelerated {
void assert_fail(const char *assertion, const char *fn, unsigned int line, const char *func);
}

// Make sure these checks are always enabled. Especially Android likes to
// define NDEBUG by default
#define aa_assert(expr) (static_cast <bool> (expr) ? void (0) : \
    accelerated::assert_fail (#expr, __FILE__, __LINE__, __PRETTY_FUNCTION__))
