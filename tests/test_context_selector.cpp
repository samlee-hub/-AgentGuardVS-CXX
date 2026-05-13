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
} // namespace
