#include <stdexcept>
#include <iostream>

#include "application.hpp"

int main()
{
    Application application = {};

    try
    {
        application.main_loop();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
    