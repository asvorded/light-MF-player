//#pragma once
//
//#include <Windows.h>
//
//class SeekBar {
//private:
//    HWND hWnd;
//
//    POINT pos; // x - constant
//    SIZE size;
//    SIZE origin; // x - dist from lower bound, y - dist from right bound
//    
//    RECT invalidRgn;
//
//    BOOL isActive;
//    LONG cuurrSeekPos;
//
//    COLORREF barColor = COLOR_HIGHLIGHTTEXT;
//    COLORREF pastColor = COLOR_HOTLIGHT;
//    COLORREF inactiveColor = COLOR_HOTLIGHT;
//    HICON hSeekPointIcon;
//public:
//    SeekBar(HINSTANCE hInst, HWND hWnd, CPlayer *pOwnerPlayer, LONG x, LONG y);
//    ~SeekBar() {};
//
//    // Creating and destroying
//    //static void* CreateSeekBar(HINSTANCE hInst, DWORD x, DWORD y, DWORD width, DWORD height);
//    //HRESULT DestroySeekBar();
//
//    // Manage seek bar state
//    HRESULT SetActive();
//    HRESULT SetInactive();
//    
//    // Repainting the control in response to events
//
//    HRESULT OnTimer();
//    HRESULT Repaint(HDC hdc);
//
//    // Event handlers
//
//    HRESULT OnWindowSizeChanged();
//    HRESULT OnPlaybackTick();
//
//    HRESULT OnMouseDown();
//    HRESULT OnMouseMove();
//    HRESULT OnMouseUp();
//
//    HRESULT OnInactive();
//};