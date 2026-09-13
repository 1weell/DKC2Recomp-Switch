#include "windows_menu.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

typedef struct MenuItem {
  wchar_t text[128];
  bool top, submenu;
} MenuItem;

struct Dkc2WindowsMenu {
  HWND window;
  HMENU bar;
  Dkc2MenuState previous;
  bool has_previous;
  HBRUSH background;
  HFONT font;
  bool in_menu, blocked_keys[256];
  unsigned commands[16], read_command, write_command;
  MenuItem items[96];
  unsigned item_count;
};

static const COLORREF background_color = RGB(28, 29, 33);
static const COLORREF foreground_color = RGB(235, 235, 240);

static void ThemeTitlebar(HWND window) {
  /* Attribute 20 is Windows 10; optional caption/text colors need Windows 11. */
  BOOL dark = TRUE;
  DwmSetWindowAttribute(window, 20, &dark, sizeof dark);
  DwmSetWindowAttribute(window, 35, &background_color, sizeof background_color);
  DwmSetWindowAttribute(window, 36, &foreground_color, sizeof foreground_color);
}

static void CaptureHeldKeys(Dkc2WindowsMenu *menu) {
  for (int key = VK_BACK; key < 256; ++key)
    menu->blocked_keys[key] = (GetAsyncKeyState(key) & 0x8000) != 0;
}

/* Owner drawing uses documented Win32 APIs; no private uxtheme ordinals. */
static LRESULT CALLBACK MenuProc(HWND window, UINT message, WPARAM wparam,
                                LPARAM lparam, UINT_PTR id, DWORD_PTR data) {
  Dkc2WindowsMenu *menu = (Dkc2WindowsMenu *)data;
  (void)id;
  if (message == WM_ENTERMENULOOP) {
    menu->in_menu = true;
    CaptureHeldKeys(menu);
  } else if (message == WM_EXITMENULOOP) {
    menu->in_menu = false;
    CaptureHeldKeys(menu);
  } else if (message == WM_KILLFOCUS) {
    CaptureHeldKeys(menu);
  } else if (message == WM_COMMAND && lparam == 0 && HIWORD(wparam) == 0) {
    unsigned command = LOWORD(wparam);
    UINT state = GetMenuState(menu->bar, command, MF_BYCOMMAND);
    if (state != (UINT)-1 && !(state & (MF_DISABLED | MF_GRAYED))) {
      if (menu->write_command - menu->read_command < 16)
        menu->commands[menu->write_command++ % 16] = command;
      return 0;
    }
  }
  if (message == WM_MEASUREITEM && wparam == 0) {
    MEASUREITEMSTRUCT *measure = (MEASUREITEMSTRUCT *)lparam;
    if (measure->CtlType == ODT_MENU && measure->itemData) {
      MenuItem *item = (MenuItem *)measure->itemData;
      HDC dc = GetDC(window);
      HGDIOBJ previous = SelectObject(dc, menu->font);
      SIZE size = {0};
      GetTextExtentPoint32W(dc, item->text, (int)wcslen(item->text), &size);
      measure->itemWidth = (UINT)(size.cx + (item->top ? 12 : 64));
      measure->itemHeight = (UINT)(size.cy + 10);
      SelectObject(dc, previous);
      ReleaseDC(window, dc);
      return TRUE;
    }
  } else if (message == WM_DRAWITEM && wparam == 0) {
    DRAWITEMSTRUCT *draw = (DRAWITEMSTRUCT *)lparam;
    if (draw->CtlType == ODT_MENU && draw->itemData) {
      MenuItem *item = (MenuItem *)draw->itemData;
      int saved = SaveDC(draw->hDC);
      SetDCBrushColor(draw->hDC, (draw->itemState & (ODS_SELECTED | ODS_HOTLIGHT))
          ? RGB(64, 66, 76) : background_color);
      FillRect(draw->hDC, &draw->rcItem, (HBRUSH)GetStockObject(DC_BRUSH));
      SetBkMode(draw->hDC, TRANSPARENT);
      SetTextColor(draw->hDC, (draw->itemState & ODS_DISABLED)
          ? RGB(135, 135, 145) : foreground_color);
      SelectObject(draw->hDC, menu->font);
      RECT text = draw->rcItem;
      text.left += item->top ? 6 : 28;
      text.right -= item->top ? 6 : 24;
      wchar_t label[128];
      wcscpy_s(label, 128, item->text);
      wchar_t *shortcut = wcschr(label, L'\t');
      if (shortcut) *shortcut++ = L'\0';
      UINT flags = DT_SINGLELINE | DT_VCENTER;
      if (draw->itemState & ODS_NOACCEL) flags |= DT_HIDEPREFIX;
      DrawTextW(draw->hDC, label, -1, &text, flags);
      if (shortcut) DrawTextW(draw->hDC, shortcut, -1, &text, flags | DT_RIGHT);
      if (draw->itemState & ODS_CHECKED) {
        RECT mark = draw->rcItem;
        mark.right = mark.left + 24;
        DrawTextW(draw->hDC, L"\x2713", 1, &mark, flags | DT_CENTER);
      }
      if (item->submenu && !item->top) {
        RECT arrow = draw->rcItem;
        arrow.left = arrow.right - 20;
        DrawTextW(draw->hDC, L"\x203a", 1, &arrow, flags | DT_CENTER);
      }
      RestoreDC(draw->hDC, saved);
      return TRUE;
    }
  } else if (message == WM_MENUCHAR) {
    HMENU popup = (HMENU)lparam;
    for (int i = 0; i < GetMenuItemCount(popup); ++i) {
      MENUITEMINFOW info = {sizeof info};
      info.fMask = MIIM_DATA;
      if (GetMenuItemInfoW(popup, (UINT)i, TRUE, &info) && info.dwItemData) {
        MenuItem *item = (MenuItem *)info.dwItemData;
        const wchar_t *key = wcschr(item->text, L'&');
        if (key && towupper(key[1]) == towupper((wchar_t)LOWORD(wparam)))
          return MAKELRESULT(i, MNC_EXECUTE);
      }
    }
  }
  return DefSubclassProc(window, message, wparam, lparam);
}

static bool ThemeMenu(Dkc2WindowsMenu *menu, HMENU native, bool top) {
  MENUINFO background = {sizeof background};
  background.fMask = MIM_BACKGROUND;
  background.hbrBack = menu->background;
  if (!SetMenuInfo(native, &background)) return false;
  for (int i = 0; i < GetMenuItemCount(native); ++i) {
    if (menu->item_count >= 96) return false;
    MenuItem *item = &menu->items[menu->item_count++];
    HMENU popup = GetSubMenu(native, i);
    item->top = top;
    item->submenu = popup != NULL;
    if (!GetMenuStringW(native, (UINT)i, item->text, 128, MF_BYPOSITION)) return false;
    MENUITEMINFOW info = {sizeof info};
    info.fMask = MIIM_FTYPE | MIIM_DATA;
    info.fType = MFT_OWNERDRAW;
    info.dwItemData = (ULONG_PTR)item;
    if (!SetMenuItemInfoW(native, (UINT)i, TRUE, &info)) return false;
    if (popup && !ThemeMenu(menu, popup, false)) return false;
  }
  return true;
}

static bool Add(HMENU menu, unsigned id, const wchar_t *label) {
  return AppendMenuW(menu, MF_STRING, id, label) != 0;
}

static bool Choices(HMENU parent, const wchar_t *title, unsigned first,
                    const wchar_t *const *labels, unsigned count) {
  HMENU menu = CreatePopupMenu();
  if (!menu) return false;
  for (unsigned i = 0; i < count; ++i) {
    if (!Add(menu, first + i, labels[i])) {
      DestroyMenu(menu); return false;
    }
  }
  if (!AppendMenuW(parent, MF_POPUP, (UINT_PTR)menu, title)) {
    DestroyMenu(menu); return false;
  }
  return true;
}

Dkc2WindowsMenu *Dkc2WindowsMenuCreate(void *native_window) {
  if (!native_window || !IsWindow((HWND)native_window)) return NULL;
  Dkc2WindowsMenu *menu = calloc(1, sizeof *menu);
  if (!menu) return NULL;
  menu->window = (HWND)native_window;
  menu->background = CreateSolidBrush(background_color);
  NONCLIENTMETRICSW metrics = {sizeof metrics};
  if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof metrics, &metrics, 0))
    menu->font = CreateFontIndirectW(&metrics.lfMenuFont);
  if (!menu->background || !menu->font ||
      !SetWindowSubclass(menu->window, MenuProc, 1, (DWORD_PTR)menu)) {
    Dkc2WindowsMenuDestroy(menu); return NULL;
  }
  ThemeTitlebar(menu->window);
  menu->bar = CreateMenu();
  HMENU game = CreatePopupMenu(), view = CreatePopupMenu();
  if (!menu->bar || !game || !view) {
    if (game) DestroyMenu(game);
    if (view) DestroyMenu(view);
    Dkc2WindowsMenuDestroy(menu); return NULL;
  }
  if (!AppendMenuW(menu->bar, MF_POPUP, (UINT_PTR)game, L"&Game")) {
    DestroyMenu(game); DestroyMenu(view);
    Dkc2WindowsMenuDestroy(menu); return NULL;
  }
  if (!AppendMenuW(menu->bar, MF_POPUP, (UINT_PTR)view, L"&View")) {
    DestroyMenu(view); Dkc2WindowsMenuDestroy(menu); return NULL;
  }
  static const wchar_t *const aspects[] = {
    L"Native 4:3 (256x224)", L"Widescreen 16:10 (308x224)",
    L"Widescreen 16:9 (342x224)", L"Ultrawide 21:9 (446x224)"};
  static const wchar_t *const scalers[] = {
    L"Pixel Sharp (Nearest)", L"Smooth (Bilinear)", L"Reconstruct (experimental)"};
  static const wchar_t *const modes[] = {
    L"Sharp pixels only", L"+ Dither decoding", L"+ Diagonal edges",
    L"+ Level-2 slopes", L"+ Level-3 slopes"};
  static const wchar_t *const edges[] = {
    L"Reflect terrain past the wall", L"Black past the wall",
    L"Shift view inward at the wall", L"Glide view inward at the wall"};
  static const wchar_t *const screens[] = {L"Raw", L"CRT", L"Composite", L"Trinitron"};
  bool ok = Add(game, kDkc2MenuPause, L"&Pause / Resume\tEsc") &&
      Add(game, kDkc2MenuSettings, L"&Settings / Controls...") &&
      Add(game, kDkc2MenuSave, L"Quick S&ave State") &&
      Add(game, kDkc2MenuLoad, L"Quick &Load State") &&
      Add(game, kDkc2MenuAbout, L"&About DKC2Recomp") &&
      Add(game, kDkc2MenuQuit, L"&Quit\tAlt+F4") &&
      Add(view, kDkc2MenuFullscreen, L"Toggle &Full Screen\tAlt+Enter") &&
      Choices(view, L"&Aspect Ratio", kDkc2MenuAspect, aspects, 4) &&
      Choices(view, L"&Scaling", kDkc2MenuUpscaler, scalers, 3) &&
      Choices(view, L"&Reconstruction Mode", kDkc2MenuReconstruct, modes, 5) &&
      Choices(view, L"&Level Edge", kDkc2MenuEdge, edges, 4) &&
      Choices(view, L"Screen &Model", kDkc2MenuScreen, screens, 4);
  HMENU input = CreatePopupMenu();
  static const wchar_t *const sources[] = {L"None", L"Keyboard", L"Gamepad"};
  static const wchar_t *const coop[] = {L"Simultaneous co-op", L"Classic alternating"};
  if (!input || !AppendMenuW(menu->bar, MF_POPUP, (UINT_PTR)input, L"&Input")) {
    if (input) DestroyMenu(input);
    Dkc2WindowsMenuDestroy(menu); return NULL;
  }
  ok = ok && Choices(input, L"Player &1", kDkc2MenuPlayer1, sources, 3) &&
      Choices(input, L"Player &2", kDkc2MenuPlayer2, sources, 3) &&
      Choices(input, L"2P &Team Mode", kDkc2MenuCoop, coop, 2);
  /* Attaching a bar keeps the outer frame and takes its height from the
   * client area SDL just sized, so restore the requested client size. On a
   * fullscreen window SDL only records the windowed size for later. */
  RECT client;
  GetClientRect(menu->window, &client);
  if (!ok || !ThemeMenu(menu, menu->bar, true) || !SetMenu(menu->window, menu->bar)) {
    Dkc2WindowsMenuDestroy(menu); return NULL;
  }
  RECT outer = client;
  DWORD style = (DWORD)GetWindowLongPtrW(menu->window, GWL_STYLE);
  DWORD exstyle = (DWORD)GetWindowLongPtrW(menu->window, GWL_EXSTYLE);
  if ((style & WS_OVERLAPPEDWINDOW) &&
      AdjustWindowRectEx(&outer, style, TRUE, exstyle))
    SetWindowPos(menu->window, NULL, 0, 0, outer.right - outer.left,
        outer.bottom - outer.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  DrawMenuBar(menu->window);
  return menu;
}

void Dkc2WindowsMenuSetVisible(Dkc2WindowsMenu *menu, bool visible) {
  if (!menu || !menu->bar || !IsWindow(menu->window)) return;
  if (visible == (GetMenu(menu->window) == menu->bar)) return;
  SetMenu(menu->window, visible ? menu->bar : NULL);
  DrawMenuBar(menu->window);
}

bool Dkc2WindowsMenuIsVisible(const Dkc2WindowsMenu *menu) {
  return menu && menu->bar && IsWindow(menu->window) &&
      GetMenu(menu->window) == menu->bar;
}

void Dkc2WindowsMenuDestroy(Dkc2WindowsMenu *menu) {
  if (!menu) return;
  RemoveWindowSubclass(menu->window, MenuProc, 1);
  if (menu->bar) {
    if (IsWindow(menu->window) && GetMenu(menu->window) == menu->bar)
      SetMenu(menu->window, NULL);
    DestroyMenu(menu->bar);
  }
  if (menu->font) DeleteObject(menu->font);
  if (menu->background) DeleteObject(menu->background);
  free(menu);
}

unsigned Dkc2WindowsMenuTakeCommand(Dkc2WindowsMenu *menu) {
  if (!menu || menu->read_command == menu->write_command) return 0;
  return menu->commands[menu->read_command++ % 16];
}

bool Dkc2WindowsMenuBlocksInput(Dkc2WindowsMenu *menu) {
  if (!menu) return false;
  if (menu->in_menu || GetForegroundWindow() != menu->window) return true;
  bool blocked = false;
  for (int key = VK_BACK; key < 256; ++key) {
    menu->blocked_keys[key] = menu->blocked_keys[key] &&
        (GetAsyncKeyState(key) & 0x8000) != 0;
    blocked = blocked || menu->blocked_keys[key];
  }
  if (blocked) return true;
  return false;
}

void Dkc2WindowsMenuUpdate(Dkc2WindowsMenu *menu, const Dkc2MenuState *state) {
  if (!menu || !state) return;
  const Dkc2MenuState *old = &menu->previous;
  if (menu->has_previous && old->aspect == state->aspect &&
      old->upscaler == state->upscaler && old->reconstruct == state->reconstruct &&
      old->edge == state->edge && old->screen == state->screen &&
      old->fullscreen == state->fullscreen && old->paused == state->paused &&
      old->player_source[0] == state->player_source[0] &&
      old->player_source[1] == state->player_source[1] && old->coop == state->coop &&
      old->settings_available == state->settings_available &&
      old->reconstruct_available == state->reconstruct_available) return;
  static const unsigned starts[] = {kDkc2MenuAspect, kDkc2MenuUpscaler,
    kDkc2MenuReconstruct, kDkc2MenuEdge, kDkc2MenuScreen,
    kDkc2MenuPlayer1, kDkc2MenuPlayer2, kDkc2MenuCoop};
  static const unsigned counts[] = {4, 3, 5, 4, 4, 3, 3, 2};
  for (unsigned group = 0; group < 8; ++group)
    for (unsigned i = 0; i < counts[group]; ++i) {
      unsigned id = starts[group] + i;
      CheckMenuItem(menu->bar, id, MF_BYCOMMAND |
          (Dkc2MenuSelected(state, id) ? MF_CHECKED : MF_UNCHECKED));
    }
  CheckMenuItem(menu->bar, kDkc2MenuPause, MF_BYCOMMAND |
      (state->paused ? MF_CHECKED : MF_UNCHECKED));
  EnableMenuItem(menu->bar, kDkc2MenuSettings, MF_BYCOMMAND |
      (state->settings_available ? MF_ENABLED : MF_GRAYED));
  EnableMenuItem(menu->bar, kDkc2MenuUpscaler + 2, MF_BYCOMMAND |
      (state->reconstruct_available ? MF_ENABLED : MF_GRAYED));
  for (unsigned i = 0; i < 5; ++i)
    EnableMenuItem(menu->bar, kDkc2MenuReconstruct + i, MF_BYCOMMAND |
        (state->reconstruct_available ? MF_ENABLED : MF_GRAYED));
  MENUITEMINFOW info = {sizeof info};
  info.fMask = MIIM_DATA;
  if (GetMenuItemInfoW(menu->bar, kDkc2MenuFullscreen, FALSE, &info) && info.dwItemData) {
    MenuItem *item = (MenuItem *)info.dwItemData;
    wcscpy_s(item->text, 128, state->fullscreen ? L"Exit &Full Screen\tAlt+Enter" : L"Enter &Full Screen\tAlt+Enter");
    info.fMask = MIIM_STRING;
    info.dwTypeData = item->text;
    SetMenuItemInfoW(menu->bar, kDkc2MenuFullscreen, FALSE, &info);
  }
  menu->previous = *state;
  menu->has_previous = true;
  DrawMenuBar(menu->window);
}
