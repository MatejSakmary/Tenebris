#pragma once

#ifdef LOG_DEBUG
#include <iostream>
#define DEBUG_OUT(x) (std::cout << x << std::endl)
#define DEBUG_VAR_OUT(x) (std::cout << #x << ": " << (x) << std::endl)

#define DBG_ASSERT_TRUE_M(x, m)                                     \
    [&] {                                                           \
        if (!(x))                                                   \
        {                                                           \
            std::cerr << (m) << std::endl;                          \
            throw std::runtime_error("DEBUG ASSERTION FAILURE");    \
        }                                                           \
    }()
#else
#define DEBUG_OUT(x)
#define DEBUG_VAR_OUT(x)
#define DBG_ASSERT_TRUE_M(x, m)
#endif
