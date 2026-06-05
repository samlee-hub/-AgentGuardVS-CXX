#include "LibraryTests.h"

#include "LibrarySystem.h"

#include <iostream>
#include <string>

namespace
{
bool Expect(bool condition, const std::string& name)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << name << '\n';
        return false;
    }
    return true;
}
} // namespace

bool RunLibrarySelfTests()
{
    LibrarySystem library;
    bool ok = true;

    ok &= Expect(library.AddBook(Book{1, "Clean Code", "Robert Martin", 2, 0}), "add first book");
    ok &= Expect(!library.AddBook(Book{1, "Duplicate", "Someone", 1, 0}), "reject duplicate id");
    ok &= Expect(library.AddBook(Book{2, "The C++ Programming Language", "Bjarne Stroustrup", 1, 0}), "add second book");

    const auto clean_code = library.FindBook(1);
    ok &= Expect(clean_code.has_value() && clean_code->title == "Clean Code", "find book by id");
    ok &= Expect(library.UpdateBook(1, "Clean Code 2nd Edition", "Robert Martin", 2), "update book");
    ok &= Expect(library.BorrowBook(1), "borrow available book");
    ok &= Expect(library.BorrowBook(1), "borrow second copy");
    ok &= Expect(!library.BorrowBook(1), "reject borrow when no copies remain");
    ok &= Expect(!library.RemoveBook(1), "reject removing borrowed book");
    ok &= Expect(library.ReturnBook(1), "return borrowed book");
    ok &= Expect(library.RemoveBook(2), "remove available book");
    ok &= Expect(library.ListBooks().size() == 1, "list remaining books");

    return ok;
}
