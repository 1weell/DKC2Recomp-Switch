#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SDL_scancode.h>
#include "windows_input.h"
#include <string.h>

void Dkc2WindowsKeyDown(Dkc2WindowsKeys *keys, unsigned virtual_key) {
  if (keys && virtual_key < 256) keys->pressed[virtual_key] = true;
}

bool Dkc2WindowsKeyTapped(const Dkc2WindowsKeys *keys, unsigned virtual_key) {
  return keys && virtual_key < 256 && keys->pressed[virtual_key];
}

void Dkc2WindowsKeysClear(Dkc2WindowsKeys *keys) {
  if (keys) memset(keys, 0, sizeof *keys);
}

int Dkc2WindowsVirtualKeyFromScancode(int key) {
  if (key >= SDL_SCANCODE_A && key <= SDL_SCANCODE_Z)
    return 'A' + (int)(key - SDL_SCANCODE_A);
  if (key >= SDL_SCANCODE_1 && key <= SDL_SCANCODE_9)
    return '1' + (int)(key - SDL_SCANCODE_1);
  if (key >= SDL_SCANCODE_F1 && key <= SDL_SCANCODE_F12)
    return VK_F1 + (int)(key - SDL_SCANCODE_F1);
  if (key >= SDL_SCANCODE_F13 && key <= SDL_SCANCODE_F24)
    return VK_F13 + key - SDL_SCANCODE_F13;
  switch (key) {
    case SDL_SCANCODE_0: return '0';
    case SDL_SCANCODE_MINUS: return VK_OEM_MINUS;
    case SDL_SCANCODE_EQUALS: return VK_OEM_PLUS;
    case SDL_SCANCODE_LEFTBRACKET: return VK_OEM_4;
    case SDL_SCANCODE_RIGHTBRACKET: return VK_OEM_6;
    case SDL_SCANCODE_BACKSLASH: return VK_OEM_5;
    case SDL_SCANCODE_NONUSBACKSLASH: return VK_OEM_102;
    case SDL_SCANCODE_SEMICOLON: return VK_OEM_1;
    case SDL_SCANCODE_APOSTROPHE: return VK_OEM_7;
    case SDL_SCANCODE_GRAVE: return VK_OEM_3;
    case SDL_SCANCODE_COMMA: return VK_OEM_COMMA;
    case SDL_SCANCODE_PERIOD: return VK_OEM_PERIOD;
    case SDL_SCANCODE_SLASH: return VK_OEM_2;
    case SDL_SCANCODE_NUMLOCKCLEAR: return VK_NUMLOCK;
    case SDL_SCANCODE_APPLICATION: return VK_APPS;
    case SDL_SCANCODE_RETURN: return VK_RETURN;
    case SDL_SCANCODE_ESCAPE: return VK_ESCAPE;
    case SDL_SCANCODE_BACKSPACE: return VK_BACK;
    case SDL_SCANCODE_TAB: return VK_TAB;
    case SDL_SCANCODE_SPACE: return VK_SPACE;
    case SDL_SCANCODE_UP: return VK_UP;
    case SDL_SCANCODE_DOWN: return VK_DOWN;
    case SDL_SCANCODE_LEFT: return VK_LEFT;
    case SDL_SCANCODE_RIGHT: return VK_RIGHT;
    case SDL_SCANCODE_LSHIFT: return VK_LSHIFT;
    case SDL_SCANCODE_RSHIFT: return VK_RSHIFT;
    case SDL_SCANCODE_LCTRL: return VK_LCONTROL;
    case SDL_SCANCODE_RCTRL: return VK_RCONTROL;
    case SDL_SCANCODE_LALT: return VK_LMENU;
    case SDL_SCANCODE_RALT: return VK_RMENU;
    case SDL_SCANCODE_INSERT: return VK_INSERT;
    case SDL_SCANCODE_DELETE: return VK_DELETE;
    case SDL_SCANCODE_HOME: return VK_HOME;
    case SDL_SCANCODE_END: return VK_END;
    case SDL_SCANCODE_PAGEUP: return VK_PRIOR;
    case SDL_SCANCODE_PAGEDOWN: return VK_NEXT;
    case SDL_SCANCODE_KP_0: return VK_NUMPAD0;
    case SDL_SCANCODE_KP_1: return VK_NUMPAD1;
    case SDL_SCANCODE_KP_2: return VK_NUMPAD2;
    case SDL_SCANCODE_KP_3: return VK_NUMPAD3;
    case SDL_SCANCODE_KP_4: return VK_NUMPAD4;
    case SDL_SCANCODE_KP_5: return VK_NUMPAD5;
    case SDL_SCANCODE_KP_6: return VK_NUMPAD6;
    case SDL_SCANCODE_KP_7: return VK_NUMPAD7;
    case SDL_SCANCODE_KP_8: return VK_NUMPAD8;
    case SDL_SCANCODE_KP_9: return VK_NUMPAD9;
    case SDL_SCANCODE_KP_ENTER: return VK_RETURN;
    case SDL_SCANCODE_KP_MULTIPLY: return VK_MULTIPLY;
    case SDL_SCANCODE_KP_PLUS: return VK_ADD;
    case SDL_SCANCODE_KP_MINUS: return VK_SUBTRACT;
    case SDL_SCANCODE_KP_DECIMAL:
    case SDL_SCANCODE_KP_PERIOD: return VK_DECIMAL;
    case SDL_SCANCODE_KP_DIVIDE: return VK_DIVIDE;
    case SDL_SCANCODE_CAPSLOCK: return VK_CAPITAL;
    case SDL_SCANCODE_SCROLLLOCK: return VK_SCROLL;
    case SDL_SCANCODE_PRINTSCREEN: return VK_SNAPSHOT;
    case SDL_SCANCODE_PAUSE: return VK_PAUSE;
    case SDL_SCANCODE_LGUI: return VK_LWIN;
    case SDL_SCANCODE_RGUI: return VK_RWIN;
    default: break;
  }
  return 0;
}
