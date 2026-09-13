#include "desktop_present.h"

#include "desktop_viewport.h"

#include <string.h>

static bool EnsureBackBuffer(Dkc2DesktopPresenter *presenter, HDC target,
                             int width, int height) {
  if (presenter->back_dc && presenter->back_bitmap &&
      presenter->width == width && presenter->height == height)
    return true;

  if (!presenter->back_dc) {
    presenter->back_dc = CreateCompatibleDC(target);
    if (!presenter->back_dc) return false;
  }

  BITMAPINFO info;
  memset(&info, 0, sizeof info);
  info.bmiHeader.biSize = sizeof info.bmiHeader;
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void *back_pixels = NULL;
  HBITMAP replacement = CreateDIBSection(target, &info, DIB_RGB_COLORS,
                                         &back_pixels, NULL, 0);
  if (!replacement) return false;
  HGDIOBJ displaced = SelectObject(presenter->back_dc, replacement);
  if (!displaced || displaced == HGDI_ERROR) {
    DeleteObject(replacement);
    return false;
  }

  if (presenter->back_bitmap) {
    DeleteObject(presenter->back_bitmap);
  } else {
    presenter->original_bitmap = displaced;
  }
  presenter->back_bitmap = replacement;
  presenter->back_pixels = (uint8_t *)back_pixels;
  presenter->width = width;
  presenter->height = height;
  return true;
}

void Dkc2DesktopPresenterDestroy(Dkc2DesktopPresenter *presenter) {
  if (!presenter) return;
  if (presenter->back_dc && presenter->original_bitmap)
    SelectObject(presenter->back_dc, presenter->original_bitmap);
  if (presenter->back_bitmap) DeleteObject(presenter->back_bitmap);
  if (presenter->back_dc) DeleteDC(presenter->back_dc);
  memset(presenter, 0, sizeof *presenter);
}

bool Dkc2DesktopPresent(Dkc2DesktopPresenter *presenter, HDC target,
                        const RECT *client, const uint8_t *pixels,
                        const BITMAPINFO *bitmap_info, int source_width,
                        int source_height, bool linear_filter) {
  if (!presenter || !target || !client || !pixels || !bitmap_info ||
      source_width <= 0 || source_height <= 0)
    return false;

  int client_width = client->right - client->left;
  int client_height = client->bottom - client->top;
  if (client_width <= 0 || client_height <= 0) return true;
  if (!EnsureBackBuffer(presenter, target, client_width, client_height))
    return false;

  RECT back_rect = {0, 0, client_width, client_height};
  HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
  FillRect(presenter->back_dc, &back_rect, black);

  Dkc2DesktopViewport viewport;
  if (!Dkc2DesktopComputeViewport(client_width, client_height,
                                  source_width, source_height, &viewport))
    return false;
  SetStretchBltMode(presenter->back_dc,
                    linear_filter ? HALFTONE : COLORONCOLOR);
  if (linear_filter) SetBrushOrgEx(presenter->back_dc, 0, 0, NULL);
  if (StretchDIBits(presenter->back_dc, viewport.x, viewport.y,
                    viewport.width, viewport.height, 0, 0, source_width,
                    source_height, pixels,
                    bitmap_info, DIB_RGB_COLORS, SRCCOPY) == (int)GDI_ERROR)
    return false;

  if (presenter->overlay_draw) {
    GdiFlush();
    if (!presenter->overlay_draw(presenter->overlay_user, presenter->back_pixels,
                                 client_width, client_height)) return false;
  }
  return BitBlt(target, client->left, client->top, client_width, client_height,
                presenter->back_dc, 0, 0, SRCCOPY) != FALSE;
}
