#include <chrono>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "llm/FakeLLMProvider.h"
#include "llm/FileLLMProvider.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::FakeLLMProvider;
using agentguard::FileLLMProvider;

fs::path MakeTempFile()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() / ("agentguard_llm_" + std::to_string(stamp) + ".json");
}

TEST(LLMProviderTest, FakeProviderReturnsConfiguredTextAndJson)
{
    FakeLLMProvider provider(
        "plain-text-response",
        nlohmann::json{{"task_id", "fake-task"}});

    EXPECT_EQ(provider.GenerateText("prompt"), "plain-text-response");
    EXPECT_EQ(provider.GenerateJson("prompt").at("task_id").get<std::string>(), "fake-task");
}

TEST(LLMProviderTest, FileProviderReadsJsonFromDisk)
{
    const fs::path file_path = MakeTempFile();
    {
        std::ofstream output(file_path);
        output << R"({"changes":[{"target_file":"src/Test.cpp"}]})";
    }

    FileLLMProvider provider(file_path);

    EXPECT_NE(provider.GenerateText("prompt").find("src/Test.cpp"), std::string::npos);
    EXPECT_EQ(
        provider.GenerateJson("prompt").at("changes").at(0).at("target_file").get<std::string>(),
        "src/Test.cpp");

    fs::remove(file_path);
}
} // namespace
