#include "pch.h"
#include "counter.h"

int counter::ThreadsAlreadyCompletedCount;
int counter::ThreadsNeedToCompleteCount;

bool counter::isAllThreadsCompleteRead()
{
    return ThreadsNeedToCompleteCount == ThreadsAlreadyCompletedCount;
}
void counter::Reset()
{
    ThreadsNeedToCompleteCount = 0;
    ThreadsAlreadyCompletedCount = 0;
}