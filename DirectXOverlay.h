#pragma once

bool InitializeOverlay();
void StartOverlay();
void StopOverlay();
void UpdateOverlay(int* balls, int selected, int pocket, int combinations, float targetAngle, float currentAngle, bool editingMultiple);
void UpdateOverlayText(const char* text);
