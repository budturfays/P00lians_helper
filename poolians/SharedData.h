#pragma once
#include <mutex>

// Declare the variables as extern to be shared across files
extern float targetBallPos[3];
extern float pocketPos[3];
extern std::mutex positionMutex;
