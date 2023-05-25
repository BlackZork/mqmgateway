#pragma once

#include <chrono>

std::chrono::milliseconds
defaultWaitTime(std::chrono::steady_clock::duration toAdd = std::chrono::steady_clock::duration::zero());
