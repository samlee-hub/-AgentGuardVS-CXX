#include "diagnostics/ErrorClassifier.h"

namespace agentguard
{
ErrorCategory ClassifyErrorCode(const std::string& code)
{
    if (code == "C1083")
    {
        return ErrorCategory::IncludeError;
    }
    if (code == "C2065" || code == "C2143" || code == "C2664" || code == "C3861")
    {
        return ErrorCategory::CompileError;
    }
    if (code == "LNK2019" || code == "LNK1104")
    {
        return ErrorCategory::LinkError;
    }
    if (code.rfind("MSB", 0) == 0)
    {
        return ErrorCategory::MSBuildError;
    }
    return ErrorCategory::Unknown;
}
} // namespace agentguard
