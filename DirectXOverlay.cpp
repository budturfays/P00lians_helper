#include <d3d9.h>
#include <d3dx9.h>
#include <windows.h>
#include "DirectXOverlay.h"
#include "MinHook.h"
#include <cstdio>
#include <iostream>
#include <mutex>

ID3DXFont* pFont = NULL;
std::mutex overlayTextMutex;
char overlayText[256] = "Overlay initialized!";

typedef HRESULT(APIENTRY* EndScene_t)(LPDIRECT3DDEVICE9 pDevice);
EndScene_t oEndScene = NULL;

// Forward declaration of CleanupDirectXOverlay
void CleanupDirectXOverlay();

HRESULT APIENTRY hkEndScene(LPDIRECT3DDEVICE9 pDevice) {
    // Ensure the font is created
    if (!pFont) {
        HRESULT hr = D3DXCreateFont(pDevice, 24, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            TEXT("Arial"), &pFont);
        if (FAILED(hr)) {
            std::cerr << "Failed to create font: " << std::hex << hr << std::endl;
            return oEndScene(pDevice);
        }
    }

    // Save the current state
    IDirect3DStateBlock9* pStateBlock = NULL;
    pDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock);
    pStateBlock->Capture();

    // Set render states for overlay
    pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

    // No need to call BeginScene/EndScene here

    if (pFont) {
        RECT fontRect;
        SetRect(&fontRect, 50, 100, 500, 500);

        // Render the updated overlay text
        {
            std::lock_guard<std::mutex> lock(overlayTextMutex);
            pFont->DrawTextA(NULL, overlayText, -1, &fontRect, DT_NOCLIP,
                D3DCOLOR_ARGB(255, 255, 255, 255));
        }
    }

    // Restore the original state
    pStateBlock->Apply();
    pStateBlock->Release();

    // Call the original EndScene
    return oEndScene(pDevice);
}




DWORD_PTR* GetDeviceVTable() {
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D) return NULL;

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;

    HWND hWnd = CreateWindowA("STATIC", "Dummy", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);

    IDirect3DDevice9* pDevice = NULL;
    HRESULT hr = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice);

    if (FAILED(hr)) {
        pD3D->Release();
        DestroyWindow(hWnd);
        return NULL;
    }

    DWORD_PTR* pVTable = *(DWORD_PTR**)pDevice;

    pDevice->Release();
    pD3D->Release();
    DestroyWindow(hWnd);

    return pVTable;
}

bool HookEndScene() {
    DWORD_PTR* pVTable = GetDeviceVTable();
    if (!pVTable) {
        std::cerr << "Failed to get device VTable." << std::endl;
        return false;
    }

    void* pEndScene = (void*)pVTable[42];

    MH_STATUS status = MH_CreateHook(pEndScene, hkEndScene, (void**)&oEndScene);
    if (status != MH_OK) {
        std::cerr << "Failed to create hook: " << MH_StatusToString(status) << std::endl;
        return false;
    }

    status = MH_EnableHook(pEndScene);
    if (status != MH_OK) {
        std::cerr << "Failed to enable hook: " << MH_StatusToString(status) << std::endl;
        return false;
    }

    return true;
}

bool InitializeOverlay() {
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        std::cerr << "MH_Initialize failed: " << MH_StatusToString(status) << std::endl;
        return false;
    }

    if (!HookEndScene()) {
        std::cerr << "Failed to hook EndScene" << std::endl;
        return false;
    }

    // Start the overlay if necessary
    StartOverlay();

    return true;
}

void UpdateOverlayText(const char* text) {
    std::lock_guard<std::mutex> lock(overlayTextMutex);
    snprintf(overlayText, sizeof(overlayText), "%s", text);
}

void UpdateOverlay(int* balls, int selected, int pocket, int combinations, float targetAngle, float currentAngle, bool editingMultiple) {
    char overlayTextBuffer[512];

    int offset = snprintf(overlayTextBuffer, sizeof(overlayTextBuffer),
        "Target Pocket: %d\n"
        "Editing ball: %d\n"
        "Target Ball 1: %d\n",
        pocket, selected + 1, balls[0]);

    if (combinations >= 1) {
        offset += snprintf(overlayTextBuffer + offset, sizeof(overlayTextBuffer) - offset,
            "Target Ball 2: %d\n", balls[1]);
    }
    if (combinations == 2) {
        offset += snprintf(overlayTextBuffer + offset, sizeof(overlayTextBuffer) - offset,
            "Target Ball 3: %d\n", balls[2]);
    }

    snprintf(overlayTextBuffer + offset, sizeof(overlayTextBuffer) - offset,
        "Target Angle: %.2f\n"
        "Current Angle: %.2f",
        targetAngle, currentAngle);

    UpdateOverlayText(overlayTextBuffer);
}





void StartOverlay() {
    // Any initialization code if needed
}

void StopOverlay() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    CleanupDirectXOverlay();
}

void CleanupDirectXOverlay() {
    if (pFont) {
        pFont->Release();
        pFont = NULL;
    }
}
