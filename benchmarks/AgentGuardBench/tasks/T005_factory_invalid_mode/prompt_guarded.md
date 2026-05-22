Implement the task: invalid game mode names must fail safely with an explicit error path and no accidental fallback mode creation.

Allowed files:
- `Server1/Server1/Server/GameModeFactory.cpp`
- `Server1/Server1/Server/GameModeFactory.h`
- `Server1/Server1/Server/BattleRoom.cpp`

Forbidden files:
- `Server1/Server1/Server/StrategyBattleMode.cpp`
- `Server1/Server1/Server/RacingMode.cpp`
- `Server1/Server1/Server/MatchManager.cpp`
- `Client1/Client1/Client/Client.cpp`

First output a PatchPlan. Edit only allowed files. If another file is required, stop and explain why. Build after editing if possible, repair only in allowed files, and finish with summary, build result, and risks.
