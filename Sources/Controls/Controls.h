#pragma once

#include <Windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <shlwapi.h>

#define WM_APP_CONTROLS    WM_APP + 50
#define CONTROLS_REPEAT    0
#define CONTROLS_PREVIOUS  1
#define CONTROLS_PAUSE     2
#define CONTROLS_NEXT      3
#define CONTROLS_SHUFFLE   4

#define REPEAT_ENABLED     10
#define REPEAT_ONE         11
#define REPEAT_NONE        12


using namespace Gdiplus;

namespace Controls {

    void InitControls(HINSTANCE hInst, HWND hWnd);
    int  Controls_EnablePlay();
    int  Controls_DisablePlay();
    int Controls_SwitchButton(int i);
    void Controls_HitTest(LPARAM lParam);
    void Controls_OnMouseDown();
    void Controls_OnMouseUp();
    void Controls_OnWindowSizeChanged();
    void Controls_Draw(HDC hdc);
    

}