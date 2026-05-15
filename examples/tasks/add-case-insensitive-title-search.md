# Task: Add Case-Insensitive Title Search

Modify the LibrarySystem demo so users can search books by a title keyword.

Requirements:

- Add a method to `LibrarySystem` that returns books whose title contains a keyword.
- Matching must be case-insensitive.
- Empty keywords must return no results.
- Existing add, remove, update, find, borrow, return, and list behavior must keep working.
- Add console self-tests for the new search behavior.
- Build verification must pass.

Expected files:

- `LibraryApp/LibrarySystem.h`
- `LibraryApp/LibrarySystem.cpp`
- `LibraryApp/LibraryTests.cpp`

Do not modify the original demo project directly when using AgentGuardVS-CXX. Apply changes only inside the isolated workspace returned by analyze.
