#include "desktop_menu.h"
#include "windows_menu.h"
#include <SDL.h>
#include <SDL_syswm.h>
#include <stdio.h>

#define CHECK(value) do { if (!(value)) { \
  fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); return 1; \
} } while (0)

int main(void) {
  Dkc2MenuState state = {0, 0, 3, 3, 0, false};
  static const unsigned starts[] = {kDkc2MenuAspect, kDkc2MenuUpscaler,
      kDkc2MenuReconstruct, kDkc2MenuEdge, kDkc2MenuScreen};
  static const unsigned counts[] = {4, 3, 5, 4, 4};
  for (unsigned group = 0; group < 5; ++group) {
    for (unsigned i = 0; i < counts[group]; ++i) {
      CHECK(Dkc2MenuApply(&state, starts[group] + i));
      for (unsigned j = 0; j < counts[group]; ++j)
        CHECK(Dkc2MenuSelected(&state, starts[group] + j) == (j == i));
    }
    CHECK(!Dkc2MenuApply(&state, starts[group] + counts[group]));
  }
  CHECK(state.upscaler == 2 && state.reconstruct == 4);
  CHECK(!Dkc2MenuApply(&state, kDkc2MenuSave));
  CHECK(!Dkc2MenuApply(&state, 0));
  CHECK(!Dkc2MenuApply(NULL, kDkc2MenuAspect));
  CHECK(!Dkc2MenuSelected(NULL, kDkc2MenuAspect));

  SDL_SetMainReady();
  CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_Window *window = SDL_CreateWindow("Synthetic Windows menu test",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_HIDDEN);
  CHECK(window != NULL);
  SDL_SysWMinfo native;
  SDL_VERSION(&native.version);
  CHECK(SDL_GetWindowWMInfo(window, &native));
  Dkc2WindowsMenu *menu = Dkc2WindowsMenuCreate(native.info.win.window);
  CHECK(menu != NULL);
  SDL_SetWindowSize(window, 640, 480);
  /* The bar must not take its height from the client SDL was asked for. */
  int client_width = 0, client_height = 0;
  SDL_GetWindowSize(window, &client_width, &client_height);
  CHECK(client_width == 640 && client_height == 480);
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  CHECK(SDL_GetWindowWMInfo(window, &info));
  HWND hwnd = info.info.win.window;
  HMENU bar = GetMenu(hwnd);
  CHECK(bar != NULL && GetMenuItemCount(bar) == 3);
  CHECK(GetMenuItemCount(GetSubMenu(bar, 0)) == 6);
  CHECK(GetMenuItemCount(GetSubMenu(bar, 1)) == 6);
  MENUINFO background = {sizeof background};
  background.fMask = MIM_BACKGROUND;
  CHECK(GetMenuInfo(bar, &background));
  LOGBRUSH brush;
  CHECK(GetObjectW(background.hbrBack, sizeof brush, &brush));
  CHECK(brush.lbColor == RGB(28, 29, 33));
  MENUITEMINFOW themed = {sizeof themed};
  themed.fMask = MIIM_FTYPE | MIIM_DATA;
  CHECK(GetMenuItemInfoW(bar, kDkc2MenuFullscreen, FALSE, &themed));
  CHECK(themed.fType & MFT_OWNERDRAW);
  CHECK(themed.dwItemData != 0);
  MEASUREITEMSTRUCT measure = {0};
  measure.CtlType = ODT_MENU;
  measure.itemData = themed.dwItemData;
  CHECK(SendMessageW(hwnd, WM_MEASUREITEM, 0, (LPARAM)&measure));
  CHECK(measure.itemWidth > 64 && measure.itemHeight > 10);
  HDC dc = CreateCompatibleDC(NULL);
  HBITMAP bitmap = CreateBitmap(400, 40, 1, 32, NULL);
  HGDIOBJ previous_bitmap = SelectObject(dc, bitmap);
  DRAWITEMSTRUCT draw = {0};
  draw.CtlType = ODT_MENU;
  draw.itemData = themed.dwItemData;
  draw.hDC = dc;
  draw.rcItem.right = 400;
  draw.rcItem.bottom = 40;
  CHECK(SendMessageW(hwnd, WM_DRAWITEM, 0, (LPARAM)&draw));
  CHECK(GetPixel(dc, 1, 1) == RGB(28, 29, 33));
  SelectObject(dc, previous_bitmap);
  DeleteObject(bitmap);
  DeleteDC(dc);
  CHECK(HIWORD(SendMessageW(hwnd, WM_MENUCHAR, L'v', (LPARAM)bar)) == MNC_EXECUTE);
  state.reconstruct_available = true;
  state.settings_available = true;
  Dkc2WindowsMenuUpdate(menu, &state);
  CHECK(GetMenuItemInfoW(bar, kDkc2MenuFullscreen, FALSE, &themed));
  CHECK(themed.fType & MFT_OWNERDRAW);
  CHECK(GetMenuState(bar, kDkc2MenuAspect + 3, MF_BYCOMMAND) & MF_CHECKED);
  CHECK(!(GetMenuState(bar, kDkc2MenuAspect, MF_BYCOMMAND) & MF_CHECKED));
  for (unsigned group = 0; group < 5; ++group)
    for (unsigned i = 0; i < counts[group]; ++i)
      CHECK(GetMenuState(bar, starts[group] + i, MF_BYCOMMAND) != (UINT)-1);

  /* Exercise the actual HWND -> SDL system-message -> host command route. */
  CHECK(PostMessageW(hwnd, WM_COMMAND, kDkc2MenuAspect + 1, 0));
  unsigned selected = 0;
  for (unsigned tick = 0; tick < 100 && !selected; ++tick) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      unsigned command = Dkc2WindowsMenuTakeCommand(menu);
      if (command) selected = command;
    }
    if (!selected) SDL_Delay(2);
  }
  CHECK(selected == kDkc2MenuAspect + 1);
  CHECK(Dkc2MenuApply(&state, selected));
  Dkc2WindowsMenuUpdate(menu, &state);
  CHECK(GetMenuState(bar, selected, MF_BYCOMMAND) & MF_CHECKED);
  CHECK(!(GetMenuState(bar, kDkc2MenuAspect + 3, MF_BYCOMMAND) & MF_CHECKED));

  /* Fullscreen hides the bar without losing the menu, its state or routing. */
  CHECK(Dkc2WindowsMenuIsVisible(menu));
  Dkc2WindowsMenuSetVisible(menu, false);
  CHECK(!Dkc2WindowsMenuIsVisible(menu) && GetMenu(hwnd) == NULL);
  state.fullscreen = true;
  Dkc2WindowsMenuUpdate(menu, &state);
  CHECK(PostMessageW(hwnd, WM_COMMAND, kDkc2MenuFullscreen, 0));
  selected = 0;
  for (unsigned tick = 0; tick < 100 && !selected; ++tick) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      unsigned command = Dkc2WindowsMenuTakeCommand(menu);
      if (command) selected = command;
    }
    if (!selected) SDL_Delay(2);
  }
  CHECK(selected == kDkc2MenuFullscreen);
  Dkc2WindowsMenuSetVisible(menu, true);
  CHECK(Dkc2WindowsMenuIsVisible(menu) && GetMenu(hwnd) == bar);
  CHECK(GetMenuState(bar, kDkc2MenuAspect + 1, MF_BYCOMMAND) & MF_CHECKED);
  CHECK(GetMenuItemInfoW(bar, kDkc2MenuFullscreen, FALSE, &themed));
  CHECK(wcscmp(((const wchar_t *)themed.dwItemData), L"Exit &Full Screen\tAlt+Enter") == 0);
  /* Commands from controls, disabled entries and unknown IDs never dispatch. */
  SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(kDkc2MenuSave, 1), 0);
  SendMessageW(hwnd, WM_COMMAND, kDkc2MenuSave, (LPARAM)hwnd);
  SendMessageW(hwnd, WM_COMMAND, 65530, 0);
  CHECK(Dkc2WindowsMenuTakeCommand(menu) == 0);
  state.reconstruct_available = false;
  state.settings_available = false;
  state.paused = true;
  state.player_source[0] = 2;
  state.player_source[1] = 1;
  state.coop = 1;
  Dkc2WindowsMenuUpdate(menu, &state);
  CHECK(GetMenuState(bar, kDkc2MenuPause, MF_BYCOMMAND) & MF_CHECKED);
  CHECK(GetMenuState(bar, kDkc2MenuPlayer1 + 2, MF_BYCOMMAND) & MF_CHECKED);
  CHECK(GetMenuState(bar, kDkc2MenuPlayer2 + 1, MF_BYCOMMAND) & MF_CHECKED);
  CHECK(GetMenuState(bar, kDkc2MenuCoop + 1, MF_BYCOMMAND) & MF_CHECKED);
  SendMessageW(hwnd, WM_COMMAND, kDkc2MenuUpscaler + 2, 0);
  SendMessageW(hwnd, WM_COMMAND, kDkc2MenuSettings, 0);
  CHECK(Dkc2WindowsMenuTakeCommand(menu) == 0);
  SendMessageW(hwnd, WM_COMMAND, kDkc2MenuSave, 0);
  SendMessageW(hwnd, WM_COMMAND, kDkc2MenuLoad, 0);
  CHECK(Dkc2WindowsMenuTakeCommand(menu) == kDkc2MenuSave);
  CHECK(Dkc2WindowsMenuTakeCommand(menu) == kDkc2MenuLoad);
  CHECK(Dkc2WindowsMenuTakeCommand(menu) == 0);
  SendMessageW(hwnd, WM_ENTERMENULOOP, 0, 0);
  CHECK(Dkc2WindowsMenuBlocksInput(menu));
  SendMessageW(hwnd, WM_EXITMENULOOP, 0, 0);
  CHECK(Dkc2WindowsMenuBlocksInput(menu)); /* hidden, therefore unfocused */
  Dkc2WindowsMenuDestroy(menu);
  CHECK(GetMenu(hwnd) == NULL);
  SDL_DestroyWindow(window);
  SDL_Quit();
  puts("Windows menu model, hierarchy, checkmarks and native command delivery passed");
  return 0;
}
