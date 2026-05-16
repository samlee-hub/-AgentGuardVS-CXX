# Task: Fix Return Stock Count

Modify the LibrarySystem demo so returning a book keeps stock counters consistent.

Requirements:

- Returning a missing book must return false.
- Returning a book with zero borrowed copies must return false.
- Returning a borrowed book must decrement `borrowed_count` exactly once.
- `total_count` must not be changed by return behavior.
- Existing add, remove, update, find, borrow, list, and self-test behavior must keep working.
- Add or update console self-tests for return-count behavior.
- Build verification must pass.

Expected files:

- `LibraryApp/LibrarySystem.cpp`
- `LibraryApp/LibraryTests.cpp`

Do not modify the original demo project directly when using AgentGuardVS-CXX. Apply changes only inside the isolated workspace returned by analyze.
