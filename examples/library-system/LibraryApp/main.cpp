#include "LibraryTests.h"

#include <iostream>

int main()
{
    if (!RunLibrarySelfTests())
    {
        std::cerr << "Library self-tests failed.\n";
        return 1;
    }

    std::cout << "Library self-tests passed.\n";
    return 0;
}
