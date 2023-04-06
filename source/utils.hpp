#pragma once

#ifdef LOG_DEBUG
#include <iostream>
#define DEBUG_OUT(x) (std::cout << x << std::endl)
#define DEBUG_VAR_OUT(x) (std::cout << #x << ": " << (x) << std::endl)
#include <cassert>
// Use (void) to silence unused warnings.
#define ASSERT_MSG(exp, msg) assert(((void)msg, exp))
#else
#define DEBUG_OUT(x)
#define DEBUG_VAR_OUT(x)
#define ASSERT_MSG(exp, msg)
#endif
