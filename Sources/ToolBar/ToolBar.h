#pragma once

#include <Windows.h>
#include <windowsx.h>
#include <gdiplus.h>

#define WM_APP_TOOLBAR WM_APP + 1

#define TOOL_ADD         0
#define TOOL_REMOVE      1
#define TOOL_RANDOM      2
#define TOOL_SORT        3

namespace ToolBar {

    void InitToolBar(HINSTANCE hInst, HWND hwnd);
    void ToolBar_Draw(HDC hdc);
    void ToolBar_HitTest(LPARAM lParam);
    void ToolBar_OnMouseDown();
    void ToolBar_OnMouseUp();
    void ToolBar_OnWindowSizeChanged();

}