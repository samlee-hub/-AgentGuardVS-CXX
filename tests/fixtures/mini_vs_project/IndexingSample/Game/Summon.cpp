#include "Summon.h"
#include <vector>

class SummonController
{
};

struct SummonCost
{
};

enum class SummonState
{
    Ready,
    CoolingDown
};

bool CanSummon(int currentResource)
{
    return currentResource > 0;
}

void ResetCooldown()
{
}
