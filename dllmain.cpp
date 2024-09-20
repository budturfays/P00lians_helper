#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <TlHelp32.h>
#include <cmath>
#include "DirectXOverlay.h"
#include "MinHook.h"

#if defined(_WIN64)
#pragma comment(lib, "MinHook.x64.lib")
#else
#pragma comment(lib, "MinHook.x86.lib")
#endif

bool editingMultiple = false;
int combinations = 0;
static bool prevMKeyState = false;
bool IsGameActive();
uintptr_t GetValidMemoryAddress(uintptr_t baseAddress);

bool IsGameActive() {
    // Placeholder, you should implement a check that returns true if the game is running
    return true;
}

uintptr_t GetValidMemoryAddress(uintptr_t baseAddress) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)baseAddress, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_NOACCESS)) {
            return baseAddress;
        }
    }
    std::cerr << "Invalid memory access detected!" << std::endl;
    return 0;
}

DWORD WINAPI MainThread(LPVOID param) {
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

    std::cout << "Console allocated successfully." << std::endl;

    // Initialize overlay without waiting for the device
    if (!InitializeOverlay()) {
        std::cerr << "Failed to initialize overlay" << std::endl;
        FreeConsole();
        FreeLibraryAndExitThread((HMODULE)param, 0);
        return 0;
    }

    std::cout << "Overlay hooks initialized successfully." << std::endl;

    // Wait for the game module to load
    HMODULE hGameModule = NULL;
    while (!hGameModule) {
        hGameModule = GetModuleHandle(L"game.dll");
        Sleep(100);
    }
    uintptr_t moduleBase = (uintptr_t)hGameModule;
    if (!moduleBase) {
        std::cerr << "Failed to get module base, exiting..." << std::endl;
        FreeConsole();
        FreeLibraryAndExitThread((HMODULE)param, 0);
        return 0;
    }

    uintptr_t baseAddress = GetValidMemoryAddress(moduleBase + 0x153984);
    if (baseAddress == 0) {
        std::cerr << "Invalid base memory address, exiting..." << std::endl;
        FreeConsole();
        FreeLibraryAndExitThread((HMODULE)param, 0);
        return 0;
    }

    std::cout << "Memory address is valid, continuing with logic.\nPress ins to stop before exit." << std::endl;

    // Main game logic variables
    int solidsOrStripes = 0;
    int balls[4] = { 1, 1, 1, 1 };
    int pocketNum = 0;
    int combinations = 0;
    int selectedBall = 0;
    float targetAngle = 0;
    float currentAngle = 0;

    const float pockets[6][2] = {
        {-0.525f, 1.0f},
        {-0.55f, 0.0f},
        {-0.525f, -1.0f},
        {0.525f, -1.0f},
        {0.55f, 0.0f},
        {0.525f, 1.0f}
    };

    // Validate ballsBaseAddress and cueBallPtr
    uintptr_t ballsBaseAddress = *(uintptr_t*)(baseAddress)+0x1EB4;
    uintptr_t cueBallPtr = *(uintptr_t*)(ballsBaseAddress);
    uintptr_t cueBall = *(uintptr_t*)(cueBallPtr);

    // Main loop - breaks when the Insert key is pressed
    while (!GetAsyncKeyState(VK_INSERT)) // VK_INSERT
    {
        if (!IsGameActive()) {
            std::cout << "Game round ended, exiting thread." << std::endl;
            break;
        }

        // Toggle between solids and stripes
        if (GetAsyncKeyState(0x30) & 1) { // 0 key
            solidsOrStripes = solidsOrStripes == 0 ? 8 : 0;
        }

        // Ball selection logic (1-8)
        if (GetAsyncKeyState(0x31) & 1) { balls[selectedBall] = 1 + solidsOrStripes; }
        else if (GetAsyncKeyState(0x32) & 1) { balls[selectedBall] = 2 + solidsOrStripes; }
        else if (GetAsyncKeyState(0x33) & 1) { balls[selectedBall] = 3 + solidsOrStripes; }
        else if (GetAsyncKeyState(0x34) & 1) { balls[selectedBall] = 4 + solidsOrStripes; }
        else if (GetAsyncKeyState(0x35) & 1) { balls[selectedBall] = 5 + solidsOrStripes; }
        else if (GetAsyncKeyState(0x36) & 1) { balls[selectedBall] = 6 + solidsOrStripes; }
        else if (GetAsyncKeyState(0x37) & 1) { balls[selectedBall] = 7 + solidsOrStripes; }
        else if (GetAsyncKeyState(0x38) & 1) { balls[selectedBall] = 8; }

        // Pocket selection (Z-N)
        if (GetAsyncKeyState(0x5A) & 1) { pocketNum = 0; } // Z
        else if (GetAsyncKeyState(0x58) & 1) { pocketNum = 1; } // X
        else if (GetAsyncKeyState(0x43) & 1) { pocketNum = 2; } // C
        else if (GetAsyncKeyState(0x56) & 1) { pocketNum = 3; } // V
        else if (GetAsyncKeyState(0x42) & 1) { pocketNum = 4; } // B
        else if (GetAsyncKeyState(0x4E) & 1) { pocketNum = 5; } // N

        if (GetAsyncKeyState(0x4D) & 1) { // M key pressed
            combinations++;
            if (combinations > 2) { // Cycle back to 0 after 2
                combinations = 0;
                editingMultiple = false;
            }
            else {
                editingMultiple = true;
            }
            // Ensure selectedBall does not exceed combinations
            if (selectedBall > combinations) {
                selectedBall = combinations;
            }
        }

        // Change selected ball to edit (comma key)
        if (GetAsyncKeyState(0xBC) & 1) { // ,
            selectedBall++;
            if (selectedBall > combinations) { selectedBall = 0; }
        }

        // Reset game state (R key)
        if (GetAsyncKeyState(0x52) & 1) { // R
            ballsBaseAddress = *(uintptr_t*)(baseAddress)+0x1EB4;
            cueBallPtr = *(uintptr_t*)(ballsBaseAddress);
            cueBall = *(uintptr_t*)(cueBallPtr);

            solidsOrStripes = 0;
            balls[0] = 1;
            balls[1] = 1;
            balls[2] = 1;
            balls[3] = 1;
            pocketNum = 0;
            combinations = 0;
            selectedBall = 0;
            targetAngle = 0;
            currentAngle = 0;
        }

        // Perform necessary calculations (target direction, angles, etc.)
        uintptr_t targetBallPtr = cueBallPtr + (0x4 * balls[0]);
        uintptr_t targetBall = *(uintptr_t*)(targetBallPtr);

        float targetBallPos[2] = { *(float*)(targetBall + 0x3C), *(float*)(targetBall + 0x44) };
        float direction[2] = { targetBallPos[0] - pockets[pocketNum][0], targetBallPos[1] - pockets[pocketNum][1] }; // vector to pocket
        float m = sqrtf((direction[0] * direction[0]) + (direction[1] * direction[1])); // normalize
        direction[0] /= m;
        direction[1] /= m;

        float targetPosition[2] = { targetBallPos[0] + (direction[0] * 0.0565f), targetBallPos[1] + (direction[1] * 0.0565f) };

        // Loop through combinations and update target positions
        for (int i = 1; i <= combinations; i++) {
            targetBallPtr = cueBallPtr + (0x4 * balls[i]);
            targetBall = *(uintptr_t*)(targetBallPtr);

            targetBallPos[0] = *(float*)(targetBall + 0x3C);
            targetBallPos[1] = *(float*)(targetBall + 0x44);

            direction[0] = targetBallPos[0] - targetPosition[0];
            direction[1] = targetBallPos[1] - targetPosition[1];
            m = sqrtf((direction[0] * direction[0]) + (direction[1] * direction[1]));
            direction[0] /= m;
            direction[1] /= m;

            targetPosition[0] = targetBallPos[0] + (direction[0] * 0.0565f);
            targetPosition[1] = targetBallPos[1] + (direction[1] * 0.0565f);
        }

        // Cue ball position and target angle calculations
        float cueBallPos[2] = { *(float*)(cueBall + 0x3C), *(float*)(cueBall + 0x44) };
        float targetDirection[2] = { targetPosition[0] - cueBallPos[0], targetPosition[1] - cueBallPos[1] };
        m = sqrtf((targetDirection[0] * targetDirection[0]) + (targetDirection[1] * targetDirection[1]));
        targetDirection[0] /= m;
        targetDirection[1] /= m;

        targetAngle = atan2f(targetDirection[0], targetDirection[1]) * 180.0f / 3.14159265f;

        float camX = *(float*)(*(uintptr_t*)(*(uintptr_t*)(baseAddress)+0x1CC) + 0x120);
        float camY = *(float*)(*(uintptr_t*)(*(uintptr_t*)(baseAddress)+0x1CC) + 0x128);

        currentAngle = atan2f(camX, camY) * 180.0f / 3.14159265f;

        // Update the overlay with new values
        UpdateOverlay(balls, selectedBall, pocketNum + 1, combinations, targetAngle, currentAngle, editingMultiple);

        Sleep(100); // Sleep to avoid high CPU usage
    }

    StopOverlay();
    FreeConsole();
    FreeLibraryAndExitThread((HMODULE)param, 0);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(0, 0, MainThread, hModule, 0, 0);
    }
    else if (dwReason == DLL_PROCESS_DETACH) {
        StopOverlay();
    }
    return TRUE;
}
