#pragma once

#include "Book.h"

#include <optional>
#include <string>
#include <vector>

class LibrarySystem
{
public:
    bool AddBook(const Book& book);
    bool RemoveBook(int id);
    bool UpdateBook(int id, const std::string& title, const std::string& author, int total_count);
    std::optional<Book> FindBook(int id) const;
    std::vector<Book> ListBooks() const;
    bool BorrowBook(int id);
    bool ReturnBook(int id);

private:
    std::vector<Book> books_;
};
