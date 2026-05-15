#include "LibrarySystem.h"

#include <algorithm>

namespace
{
bool IsValidBook(const Book& book)
{
    return book.id > 0 && !book.title.empty() && !book.author.empty() &&
           book.total_count >= 0 && book.borrowed_count >= 0 &&
           book.borrowed_count <= book.total_count;
}
} // namespace

bool LibrarySystem::AddBook(const Book& book)
{
    if (!IsValidBook(book) || FindBook(book.id).has_value())
    {
        return false;
    }

    books_.push_back(book);
    return true;
}

bool LibrarySystem::RemoveBook(int id)
{
    const auto it = std::find_if(books_.begin(), books_.end(), [id](const Book& book) {
        return book.id == id;
    });
    if (it == books_.end() || it->borrowed_count > 0)
    {
        return false;
    }

    books_.erase(it);
    return true;
}

bool LibrarySystem::UpdateBook(int id, const std::string& title, const std::string& author, int total_count)
{
    const auto it = std::find_if(books_.begin(), books_.end(), [id](const Book& book) {
        return book.id == id;
    });
    if (it == books_.end() || title.empty() || author.empty() || total_count < it->borrowed_count)
    {
        return false;
    }

    it->title = title;
    it->author = author;
    it->total_count = total_count;
    return true;
}

std::optional<Book> LibrarySystem::FindBook(int id) const
{
    const auto it = std::find_if(books_.begin(), books_.end(), [id](const Book& book) {
        return book.id == id;
    });
    if (it == books_.end())
    {
        return std::nullopt;
    }

    return *it;
}

std::vector<Book> LibrarySystem::ListBooks() const
{
    return books_;
}

bool LibrarySystem::BorrowBook(int id)
{
    const auto it = std::find_if(books_.begin(), books_.end(), [id](const Book& book) {
        return book.id == id;
    });
    if (it == books_.end() || it->borrowed_count >= it->total_count)
    {
        return false;
    }

    ++it->borrowed_count;
    return true;
}

bool LibrarySystem::ReturnBook(int id)
{
    const auto it = std::find_if(books_.begin(), books_.end(), [id](const Book& book) {
        return book.id == id;
    });
    if (it == books_.end() || it->borrowed_count == 0)
    {
        return false;
    }

    --it->borrowed_count;
    return true;
}
