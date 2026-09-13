#ifndef DKC2_WINDOWS_MENU_H
#define DKC2_WINDOWS_MENU_H
#include "desktop_menu.h"
typedef struct Dkc2WindowsMenu Dkc2WindowsMenu;
Dkc2WindowsMenu *Dkc2WindowsMenuCreate(void *native_window);
void Dkc2WindowsMenuDestroy(Dkc2WindowsMenu *menu);
unsigned Dkc2WindowsMenuTakeCommand(Dkc2WindowsMenu *menu);
/* Neutralize held gameplay/menu keys until released after a menu or focus change. */
bool Dkc2WindowsMenuBlocksInput(Dkc2WindowsMenu *menu);
void Dkc2WindowsMenuUpdate(Dkc2WindowsMenu *menu, const Dkc2MenuState *state);
void Dkc2WindowsMenuSetVisible(Dkc2WindowsMenu *menu, bool visible);
bool Dkc2WindowsMenuIsVisible(const Dkc2WindowsMenu *menu);
#endif
