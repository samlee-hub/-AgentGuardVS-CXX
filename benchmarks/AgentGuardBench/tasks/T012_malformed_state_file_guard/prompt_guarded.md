Implement the task: harden UE/state ingestion against malformed state text, missing fields, empty rows, and invalid numeric values.

Allowed files:
- `UE_MyProject/UE_MyProject/MyProject2/GameStateReader.cpp`
- `UE_MyProject/UE_MyProject/MyProject2/GameStateReader.h`
- `Client1/Client1/Client/Client.cpp`

Forbidden files:
- `Server1/Server1/Server/StrategyBattleMode.cpp`
- `Server1/Server1/Server/BattleRoom.cpp`
- `Server1/Server1/Server/Server.cpp`
- `UE_MyProject/UE_MyProject/MyProject2/MyProject2.Build.cs`

First output a PatchPlan. Modify only allowed files. If another file is required, stop and explain. Build after editing if possible. Repair only within allowed files. Finish with summary, build result, and risk notes.
