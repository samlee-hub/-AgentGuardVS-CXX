# Library System Demo

This is a small Visual Studio C++ project used to demonstrate AgentGuardVS-CXX.

The initial application supports:

- Adding books.
- Removing books.
- Updating book metadata.
- Finding books by id.
- Listing books.
- Borrowing books.
- Returning books.
- Running console self-tests. The executable returns nonzero when a self-test fails.

The demo is intentionally small. It verifies the AgentGuardVS-CXX workflow on a controlled Visual Studio C++ project; it does not claim coverage of enterprise-scale C++ systems.

Run the full demo from the repository root:

```powershell
.\examples\run-library-demo.ps1
```

The script builds this original project, runs the original self-test, creates an isolated workspace through AgentGuardVS-CXX, applies a sample change only inside that workspace, runs MSBuild verification, runs semantic review, and prints report paths.
