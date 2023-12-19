#include "ToolBar.h"

namespace ToolBar {

POINT pos = {};
POINT offset = {};
RECT toolRect = {};
SIZE btnSize = { 110, 25 };
int gap = 0;
int rightBound = 0;

HFONT hFont = NULL;

struct ToolButton {
    HICON hIcon;
    LPWSTR btnText;
    int textLen;
};
HBRUSH hoverBrush;
HBRUSH pressedBrush;
HBRUSH outlineBrush;

enum CurrTool { add, remove, sort, none };

CurrTool isHovering = none;
CurrTool isPressed = none;

ToolButton toolButtons[4];
// 0 - add, 1 - remove, 3 - random, 2 - sort

HWND hWnd;

void InitToolBar(HINSTANCE hInst, HWND hwnd) {
    RECT clRect;
    hWnd = hwnd;
    hoverBrush = CreateSolidBrush(RGB(222, 222, 222));
    pressedBrush = CreateSolidBrush(RGB(156, 156, 156));
    outlineBrush = CreateSolidBrush(RGB(230, 230, 230));
    toolButtons[add].hIcon = LoadIcon(hInst, L"toolBarAdd");
    toolButtons[add].btnText = (LPWSTR)L"Добавить";
    toolButtons[add].textLen = 8;
    toolButtons[remove].hIcon = LoadIcon(hInst, L"toolBarRemove");
    toolButtons[remove].btnText = (LPWSTR)L"Удалить";
    toolButtons[remove].textLen = 7;
    toolButtons[sort].hIcon = LoadIcon(hInst, L"toolBarSort");
    toolButtons[sort].btnText = (LPWSTR)L"Упорядочить";
    toolButtons[sort].textLen = 11;

    hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    GetClientRect(hWnd, &clRect);
    rightBound = clRect.right;
    offset.x = 20;
    offset.y = 100 + 37;
    pos.x = (clRect.right - clRect.left) >> 1;
    pos.y = (clRect.bottom - clRect.top) - offset.y;
    gap = 10;
    toolRect = { pos.x, pos.y, clRect.right - offset.x, pos.y + btnSize.cy };
}

void DrawOneButton(HDC hdc, POINT point, int t) {
    RECT rect = {};
    rect.left = point.x;
    rect.top = point.y;
    rect.right = rect.left + btnSize.cx;
    rect.bottom = rect.top + btnSize.cy;
    if (t == isHovering && t != isPressed)
        FillRect(hdc, &rect, hoverBrush);
    else if (t == isHovering && t == isPressed)
        FillRect(hdc, &rect, pressedBrush);
    DrawIcon(hdc, rect.left + 5, rect.top + 5, toolButtons[t].hIcon);
    FrameRect(hdc, &rect, outlineBrush);
    TextOutW(hdc, rect.left + 3 + 22, rect.top + 6, toolButtons[t].btnText, toolButtons[t].textLen);
}

void ToolBar_Draw(HDC hdc) {
    POINT point = {};
    SaveDC(hdc);
    SelectObject(hdc, hFont);
    for (int t = add; t <= remove; t++) {
        point.x = pos.x + t * (btnSize.cx + gap);
        point.y = pos.y;
        DrawOneButton(hdc, point, t);
    }
    point.x = rightBound - offset.x - btnSize.cx;
    DrawOneButton(hdc, point, sort);
    RestoreDC(hdc, -1);
}

void ToolBar_HitTest(LPARAM lParam) {
    POINT cursorPos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    LONG currX = 0;
    isHovering = none;
    if (cursorPos.x <= toolRect.right && cursorPos.y >= toolRect.top && cursorPos.y <= toolRect.bottom) {
        if (cursorPos.x >= rightBound - offset.x - btnSize.cx)
            isHovering = sort;
        else {
            currX = pos.x - cursorPos.x - gap;

            for (int t = add; t <= remove; t++) {
                currX += gap + btnSize.cx;
                if (currX >= 0 && currX <= btnSize.cx) {
                    isHovering = (CurrTool)t;
                    t = remove + 1;
                }
            }
        }
    }
    InvalidateRect(hWnd, &toolRect, FALSE);
}

void ToolBar_OnMouseDown() {
    isPressed = isHovering;
    SetCapture(hWnd);
    InvalidateRect(hWnd, &toolRect, FALSE);
}

void ToolBar_OnMouseUp() {
    if (isHovering == isPressed)
        switch (isPressed) {
        case add:
            PostMessageW(hWnd, WM_APP_TOOLBAR, TOOL_ADD, 0L);
            break;
        case remove:
            PostMessageW(hWnd, WM_APP_TOOLBAR, TOOL_REMOVE, 0L);
            break;
        case sort:
            PostMessageW(hWnd, WM_APP_TOOLBAR, TOOL_SORT, 0L);
            break;
        }
    isPressed = none;
    InvalidateRect(hWnd, &toolRect, FALSE);
}

void ToolBar_OnWindowSizeChanged() {
    RECT clRect;
    GetClientRect(hWnd, &clRect);
    rightBound = clRect.right;
    pos.x = (clRect.right - clRect.left) >> 1;
    pos.y = (clRect.bottom - clRect.top) - offset.y;
    toolRect = { pos.x, pos.y, clRect.right - offset.x, pos.y + btnSize.cy };
}

}