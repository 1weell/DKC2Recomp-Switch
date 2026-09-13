#pragma once
#include <stdbool.h>
typedef struct Dkc2WindowsKeys { bool pressed[256]; } Dkc2WindowsKeys;
int Dkc2WindowsVirtualKeyFromScancode(int scancode);
void Dkc2WindowsKeyDown(Dkc2WindowsKeys *keys, unsigned virtual_key);
bool Dkc2WindowsKeyTapped(const Dkc2WindowsKeys *keys, unsigned virtual_key);
void Dkc2WindowsKeysClear(Dkc2WindowsKeys *keys);
