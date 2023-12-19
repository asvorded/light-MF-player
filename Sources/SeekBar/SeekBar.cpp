//#include "SeekBar.h"
//
//template <class T> void SafeRelease(T** ppT) {
//    if (*ppT) {
//        delete *ppT;
//        *ppT = NULL;
//    }
//}
//
//SeekBar::SeekBar(HINSTANCE hInst, HWND hWnd, LONG x, LONG y) :
//    hWnd{ hWnd },
//    //pOwnerPlayer{ pOwnerPlayer },
//    pos{ x, 0 },
//    //origin{ x, y },
//    size{ 0, 5 },
//    cuurrSeekPos{ 0 },
//    isActive{ 0 }
//{
//    hSeekPointIcon = (HICON)LoadImageW(hInst, L"seekPoint", IMAGE_ICON, 16, 16, 0);
//    //GetLastError();
//    RECT clRect;
//    origin.cy = 80;
//    origin.cx = 50;
//    GetClientRect(hWnd, &clRect);
//    //pos.x
//    pos.y = (clRect.bottom - clRect.top) - origin.cy;
//    size.cx = (clRect.right - clRect.left) - origin.cx - pos.x;
//    //size.cy
//}
//
//// Manage seek bar
//
//HRESULT SeekBar::SetActive() {
//    isActive = 1;
//    IMFMediaSession* pMediaSession;
//    SetTimer(hWnd, TIMER_SEEKBAR, 250, NULL);
//    return S_OK;
//}
//
//HRESULT SeekBar::SetInactive() {
//    isActive = 0;
//    return S_OK;
//}
//
//// Timer procedure
//HRESULT SeekBar::OnTimer() {
//    
//}
//
//// Repainting the control in response to events
//HRESULT SeekBar::Repaint(HDC hdc) {
//    
//    RECT rect = { pos.x, pos.y, pos.x + size.cx, pos.y + size.cy };
//    if (isActive) {
//        FillRect(hdc, &rect, GetSysColorBrush(COLOR_HOTLIGHT));
//        DrawIcon(hdc, rect.left - 15, rect.top - 15, hSeekPointIcon);
//    } else {
//        FillRect(hdc, &rect, GetSysColorBrush(COLOR_HIGHLIGHTTEXT));
//    }
//        
//    return S_OK;
//}
//
//HRESULT SeekBar::OnWindowSizeChanged() {
//    RECT clRect;
//    GetClientRect(hWnd, &clRect);
//    //pos.x
//    pos.y = (clRect.bottom - clRect.top) - origin.cy;
//    size.cx = (clRect.right - clRect.left) - origin.cx - pos.x;
//    //size.cy
//
//    return S_OK;
//}
//
//HRESULT SeekBar::OnPlaybackTick()
//{
//    return E_NOTIMPL;
//}
//
//HRESULT SeekBar::OnMouseDown()
//{
//    return E_NOTIMPL;
//}
//
//HRESULT SeekBar::OnMouseMove()
//{
//    return E_NOTIMPL;
//}
//
//HRESULT SeekBar::OnMouseUp()
//{
//    return E_NOTIMPL;
//}
//
//HRESULT SeekBar::OnInactive()
//{
//    return E_NOTIMPL;
//}
