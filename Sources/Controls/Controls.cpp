#include "Controls.h"

namespace Controls {

typedef enum __buttons { repeat, previous, play, next, shuffle, pause, no_repeat, repeat_one, none } Buttons;

typedef struct __button {
    HRSRC hSource;
    HGLOBAL hGlobal;
    Image* pImage;
    IStream* pImgStream;
} Button;

HWND hWnd;

POINT pos = {};
LONG offsetY = 75;
RECT controlsRect = {};
SIZE btnSize = { 45, 45 };
SolidBrush* pHoverBrush;
SolidBrush* pPressedBrush;

Buttons isHovering;
Buttons isPressed;
BOOL isPlaying;
UINT isRepeat;
//BOOL isShuffle;

Button buttons[6 + 2]; // warning: no match actual size and elements

HRESULT InitOneControl(Button* pButton, HINSTANCE hInst, int resource) {
    LPVOID firstByte = NULL;
    UINT size = 0;
    pButton->hSource = FindResource(hInst, MAKEINTRESOURCE(resource), RT_RCDATA);
    pButton->hGlobal = LoadResource(hInst, pButton->hSource);
    firstByte = LockResource(pButton->hGlobal);
    size = SizeofResource(hInst, pButton->hSource);
    pButton->pImgStream = SHCreateMemStream((const BYTE*)firstByte, size);
    pButton->pImage = new Image{ pButton->pImgStream };
    return S_OK;
}

void InitControls(HINSTANCE hInst, HWND hwnd) {
    RECT clRect = {};
    HRSRC hSource = NULL;
    HGLOBAL hGlobal = NULL;
    IStream* pStream = NULL;
    hWnd = hwnd;
    for (int i = repeat; i <= pause; i++) {
        InitOneControl(&buttons[i], hInst, 1100 + i);
    }
    InitOneControl(&buttons[no_repeat], hInst, 1100 + no_repeat);
    InitOneControl(&buttons[repeat_one], hInst, 1100 + repeat_one);

    GetClientRect(hWnd, &clRect);
    pos.x = 20;
    pos.y = clRect.bottom - offsetY;
    controlsRect = {pos.x, pos.y, pos.x + btnSize.cx * 5, pos.y + btnSize.cy};

    isHovering = none;
    isPressed = none;
    isPlaying = FALSE;
    isRepeat = REPEAT_ENABLED;
    //isShuffle = FALSE;

    pHoverBrush = new SolidBrush{ Color(180, 180, 255) };
    pPressedBrush = new SolidBrush{ Color(160, 160, 255) };
}

int Controls_EnablePlay() {
    isPlaying = TRUE;
    InvalidateRect(hWnd, &controlsRect, FALSE);
    return isPlaying;
}

int Controls_DisablePlay() {
    isPlaying = FALSE;
    InvalidateRect(hWnd, &controlsRect, FALSE);
    return isPlaying;
}

int Controls_SwitchButton(int i) {
    switch (i) {
    case CONTROLS_REPEAT:
        if (isRepeat == REPEAT_NONE)
            isRepeat = REPEAT_ENABLED;
        else
            isRepeat++;
    }
    InvalidateRect(hWnd, &controlsRect, FALSE);
    return 0;
}

void Controls_HitTest(LPARAM lParam) {
    POINT cursorPos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    LONG currX = 0;
    isHovering = none;

    if (cursorPos.x <= controlsRect.right && cursorPos.y >= controlsRect.top && cursorPos.y <= controlsRect.bottom) {
        currX = pos.x - cursorPos.x;
        for (int b = repeat; b <= shuffle; b++) {
            currX += btnSize.cx;
            if (currX >= 0 && currX <= btnSize.cx) {
                isHovering = (Buttons)b;
                b = none + 1;
            }
        }
    }
    InvalidateRect(hWnd, &controlsRect, FALSE);
    UpdateWindow(hWnd);
}

void Controls_OnMouseDown() {
    isPressed = isHovering;
    SetCapture(hWnd);
    InvalidateRect(hWnd, &controlsRect, FALSE);
}

void Controls_OnMouseUp() {
    if (isHovering == isPressed)
        PostMessageW(hWnd, WM_APP_CONTROLS, isPressed, 0L); // isPressed = wParam number
    isPressed = none;
    InvalidateRect(hWnd, &controlsRect, FALSE);
}

void Controls_OnWindowSizeChanged() {
    RECT clRect = {};
    GetClientRect(hWnd, &clRect);
    pos.y = clRect.bottom - offsetY;
    controlsRect = { pos.x, pos.y, pos.x + btnSize.cx * 5, pos.y + btnSize.cy };
}

void Controls_Draw(HDC hdc) {
    Graphics pGraphics{ hdc };
    SolidBrush* pBrush = NULL;
    int repState = -1;
    switch (isRepeat) {
    case REPEAT_ENABLED:
        repState = repeat;
        break;
    case REPEAT_ONE:
        repState = repeat_one;
        break;
    case REPEAT_NONE:
        repState = no_repeat;
        break;
    }

    if (isHovering == isPressed)
        pBrush = pPressedBrush;
    else
        pBrush = pHoverBrush;

    if (isHovering != none) {
        pGraphics.FillRectangle(pBrush,
            (INT)pos.x + btnSize.cx * isHovering, (INT)pos.y, (INT)btnSize.cx, (INT)btnSize.cy);
    }

    for (int i = repeat; i <= shuffle; i++) {
        if (i == repeat)
            pGraphics.DrawImage(buttons[repState].pImage, 
                (INT)pos.x + btnSize.cx * i, (INT)pos.y, (INT)btnSize.cx, (INT)btnSize.cy);
        else if (i == play)
            pGraphics.DrawImage(buttons[!isPlaying ? play : pause].pImage,
                (INT)pos.x + (btnSize.cx << 1), (INT)pos.y, (INT)btnSize.cx, (INT)btnSize.cy);
        else
            pGraphics.DrawImage(buttons[i].pImage,
                (INT)pos.x + btnSize.cx * i, (INT)pos.y, (INT)btnSize.cx, (INT)btnSize.cy);
    }
}

}