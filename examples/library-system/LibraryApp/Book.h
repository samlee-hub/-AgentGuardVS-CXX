#pragma once

#include <string>

struct Book
{
    int id = 0;
    std::string title;
    std::string author;
    int total_count = 0;
    int borrowed_count = 0;
};
