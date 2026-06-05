#include <algorithm>

#include <gtest/gtest.h>

#include "indexing/ContextSelector.h"

namespace
{
using agentguard::ContextCandidate;
using agentguard::SelectRelevantFiles;
using agentguard::SourceFileInfo;

TEST(ContextSelectorTest, RanksDirectSummonMatchesAheadOfWeakerCandidates)
{
    SourceFileInfo summon;
    summon.file_path = "Game\\SummonService.cpp";
    summon.classes = {"SummonService"};
    summon.functions = {"ApplyCooldown"};
    summon.includes = {"SummonRules.h"};

    SourceFileInfo battle_room;
    battle_room.file_path = "Game\\BattleRoom.cpp";
    battle_room.functions = {"ScheduleSummonCooldown"};

    SourceFileInfo audit;
    audit.file_path = "Infra\\AuditLog.cpp";
    audit.functions = {"WriteAuditEvent"};

    const auto candidates = SelectRelevantFiles(
        "add summon cooldown handling",
        {audit, battle_room, summon});

    ASSERT_EQ(candidates.size(), 2U);
    EXPECT_EQ(candidates[0].file_path, "Game\\SummonService.cpp");
    EXPECT_GT(candidates[0].score, candidates[1].score);
    EXPECT_EQ(candidates[1].file_path, "Game\\BattleRoom.cpp");
}

TEST(ContextSelectorTest, ReportsReasonsForFilenameClassFunctionAndIncludeMatches)
{
    SourceFileInfo info;
    info.file_path = "Game\\SummonMode.cpp";
    info.classes = {"SummonController"};
    info.functions = {"ResetCooldown"};
    info.includes = {"SummonRules.h"};

    const auto candidates = SelectRelevantFiles("summon cooldown rules", {info});

    ASSERT_EQ(candidates.size(), 1U);
    const ContextCandidate& candidate = candidates.front();
    EXPECT_GT(candidate.score, 0);
    EXPECT_NE(
        std::find(candidate.reasons.begin(), candidate.reasons.end(), "filename:summon"),
        candidate.reasons.end());
    EXPECT_NE(
        std::find(candidate.reasons.begin(), candidate.reasons.end(), "class:summon"),
        candidate.reasons.end());
    EXPECT_NE(
        std::find(candidate.reasons.begin(), candidate.reasons.end(), "function:cooldown"),
        candidate.reasons.end());
    EXPECT_NE(
        std::find(candidate.reasons.begin(), candidate.reasons.end(), "include:rules"),
        candidate.reasons.end());
}

TEST(ContextSelectorTest, ChineseSummonTaskAddsDomainHints)
{
    SourceFileInfo summon;
    summon.file_path = "IndexingSample\\Game\\Summon.cpp";
    summon.classes = {"SummonController"};
    summon.functions = {"CanSummon", "ResetCooldown"};

    SourceFileInfo battle;
    battle.file_path = "IndexingSample\\Engine\\Battle.cc";
    battle.functions = {"TickBattle"};

    const std::string chinese_task =
        "\xE4\xB8\xBA\xE5\x8F\xAC\xE5\x94\xA4\xE5\x8A\x9F\xE8\x83\xBD"
        "\xE5\xA2\x9E\xE5\x8A\xA0\xE8\xB5\x84\xE6\xBA\x90\xE6\xB6\x88"
        "\xE8\x80\x97\xE5\x92\x8C\xE5\x85\xA8\xE5\xB1\x80\x33\xE7\xA7"
        "\x92\xE5\x86\xB7\xE5\x8D\xB4";

    const auto candidates = SelectRelevantFiles(chinese_task, {battle, summon});

    ASSERT_FALSE(candidates.empty());
    EXPECT_EQ(candidates.front().file_path, "IndexingSample\\Game\\Summon.cpp");
    EXPECT_NE(
        std::find(candidates.front().reasons.begin(), candidates.front().reasons.end(), "filename:summon"),
        candidates.front().reasons.end());
}
} // namespace
