#pragma once

#ifdef LOG_DEBUG
#include <iostream>
#define DEBUG_OUT(x) (std::cout << x << std::endl)
#define DEBUG_VAR_OUT(x) (std::cout << #x << ": " << (x) << std::endl)
#else
#define DEBUG_OUT(x)
#define DEBUG_VAR_OUT(x)
#endif