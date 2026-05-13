#include <gtest/gtest.h>

#include "diagnostics/ErrorParser.h"

namespace
{
using agentguard::ErrorCategory;
using agentguard::ParseBuildErrors;

TEST(ErrorParserTest, ParsesCompilerLinkerAndMsbuildCodesAcrossEnglishAndChineseLogs)
{
    const std::string log =
        "src\\Main.cpp(10,4): fatal error C1083: Cannot open include file\n"
        "src\\Main.cpp(11,2): error C2065: 'value': undeclared identifier\n"
        "src\\Main.cpp(12): error C2143: syntax error\n"
        "src\\Main.cpp(13,8): error C2664: cannot convert argument\n"
        "src\\Main.cpp(14,9): error C3861: identifier not found\n"
        "main.obj : error LNK2019: unresolved external symbol\n"
        "LINK : fatal error LNK1104: cannot open file\n"
        "Microsoft.CppCommon.targets(166,5): error MSB3073: command exited with code 1\n"
        "src\\Main.cpp(15,1): error C2065: 中文日志样例\n";

    const auto errors = ParseBuildErrors(log);

    ASSERT_EQ(errors.size(), 9U);
    EXPECT_EQ(errors[0].category, ErrorCategory::IncludeError);
    EXPECT_EQ(errors[1].category, ErrorCategory::CompileError);
    EXPECT_EQ(errors[2].category, ErrorCategory::CompileError);
    EXPECT_EQ(errors[3].category, ErrorCategory::CompileError);
    EXPECT_EQ(errors[4].category, ErrorCategory::CompileError);
    EXPECT_EQ(errors[5].category, ErrorCategory::LinkError);
    EXPECT_EQ(errors[6].category, ErrorCategory::LinkError);
    EXPECT_EQ(errors[7].category, ErrorCategory::MSBuildError);
    EXPECT_EQ(errors[8].message, "中文日志样例");
    EXPECT_EQ(errors[0].file, "src\\Main.cpp");
    EXPECT_EQ(errors[0].line, 10);
    EXPECT_EQ(errors[0].column, 4);
    EXPECT_EQ(errors[5].file, "main.obj");
    EXPECT_EQ(errors[6].code, "LNK1104");
}
} // namespace
