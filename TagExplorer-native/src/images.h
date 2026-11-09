#pragma once
#include <windows.h>
#include <wincodec.h>
HBITMAP image_thumbnail(const wchar_t *path, UINT edge);
HRESULT image_draw(HDC dc, const RECT *area, const wchar_t *path, int rotation);

HRESULT image_rotate_file(const wchar_t *path, int clockwise_degrees);
