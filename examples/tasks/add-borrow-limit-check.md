# Task: Add Borrow Limit Check

Modify the LibrarySystem demo so borrowing behavior explicitly rejects invalid or unavailable borrow attempts.

Requirements:

- Borrowing a missing book must return false.
- Borrowing a book with no available copies must return false.
- Borrowing a valid available book must increment `borrowed_count` exactly once.
- Existing add, remove, update, find, return, list, and self-test behavior must keep working.
- Add or update console self-tests for borrow-limit behavior.
- Build verification must pass.

Expected files:

- `LibraryApp/LibrarySystem.cpp`
- `LibraryApp/LibraryTests.cpp`

Do not modify the original demo project directly when using AgentGuardVS-CXX. Apply changes only inside the isolated workspace returned by analyze.
