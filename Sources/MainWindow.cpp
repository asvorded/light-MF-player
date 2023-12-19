#include <Windows.h>
#include <mmsystem.h>
#include <assert.h>
#include <shobjidl_core.h>

// ### User headers
#include "../Resources/resources.h"
// Toolbar
#include "ToolBar/ToolBar.h"
using namespace ToolBar;

// Controls
#include "Controls/Controls.h"
using namespace Controls;
using namespace Gdiplus;

// Player Client
#include "Player/Player.h"

// Playlist
#include "Playlist/Playlist.h"

// Variables and properties

// Screen buffer
HDC hdcScreenBuffer = NULL;
RECT screenBufferRect = {};
HBITMAP hScreenBufferBitmap = NULL;
HBRUSH toolBrush = NULL;

GdiplusStartupInput* pgpStandardInput;
ULONG_PTR gpStartupToken = 0;

ListFile* pCurrFile = NULL;
IStream* pThumbnail = NULL;
IStream* pImageStream = NULL;
FontFamily* pFontFamily = NULL;

RECT infoRect = { 0, 0, 300, 350 };

POINT wndOrigin = { 200, 150 };
POINT wndSize = { 900, 570 };

HMENU hSortMenu = NULL;
HMENU hPopupMenu = NULL;

CPlayer *pPlayer = NULL;
PlayList* pPlayList = NULL;
// Functions

LRESULT WINAPI WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

void CreateScreenBuffer(HWND hWnd);
void ReleaseScreenBuffer();
void ResizeScreenBuffer(HWND hWnd);
void RefreshScreenBuffer();
void DrawScreenBuffer(HDC hdc);

void DrawFileInfo(HDC hdc, ListFile* pFile);

template <class T> void SafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	MSG msg;
	HWND hWnd;
    int isStarted;
	
	const wchar_t className[] = L"BSUIRPlayer";
	WNDCLASSEXW wndClass;

	memset(&wndClass, 0, sizeof(WNDCLASSEXW));
	wndClass.cbSize = sizeof(WNDCLASSEXW);
	wndClass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = WindowProc;
	//wndClass.cbClsExtra
	//wndClass.cbWndExtra
	wndClass.hInstance = hInstance;
	wndClass.hIcon = LoadIcon(hInstance, L"mainIcon");
	wndClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)(GetStockObject(WHITE_BRUSH));
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = className;
	wndClass.hIconSm = LoadIcon(hInstance, L"mainIconSm");

	RegisterClassEx(&wndClass);

    pgpStandardInput = new GdiplusStartupInput{};
    GdiplusStartup(&gpStartupToken, pgpStandardInput, NULL);

    isStarted = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_FULL));

    // Main window
	hWnd = CreateWindowExW(0,
		className,
		L"Player",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
		wndOrigin.x, wndOrigin.y, wndSize.x, wndSize.y,
		NULL, NULL, hInstance, 0);

    // Create screen buffer
    CreateScreenBuffer(hWnd);

    // Custom components
    // Tool bar
    InitToolBar(hInstance, hWnd);
    // Controls
    InitControls(hInstance, hWnd);
    // Player
    CPlayer::CreatePlayer(&pPlayer, hInstance, hWnd);
    // Playlist
    pPlayList = PlayList::CreatePlaylist(hInstance, hWnd, 200, 200);
    
    toolBrush = CreateSolidBrush(RGB(190, 190, 255));

    hSortMenu = CreatePopupMenu();
    AppendMenuW(hSortMenu, 0, SORT_TITLE, L"По заголовку (А-Я)");
    AppendMenuW(hSortMenu, 0, SORT_TITLE_REVERSE, L"По заголовку (Я-А)");
    AppendMenuW(hSortMenu, 0, SORT_ARTIST, L"По исполнителю (А-Я)");
    AppendMenuW(hSortMenu, 0, SORT_ARTIST_REVERSE, L"По исполнителю (Я-А)");
    AppendMenuW(hSortMenu, 0, SORT_ALBUM, L"По названию альбома (А-Я)");
    AppendMenuW(hSortMenu, 0, SORT_ALBUM_REVERSE, L"По названию альбома (Я-А)");
    AppendMenuW(hSortMenu, 0, SORT_LENGTH, L"По длительности (по возрастанию)");
    AppendMenuW(hSortMenu, 0, SORT_LENGTH_REVERSE, L"По длительности (по убыванию)");
    hPopupMenu = CreatePopupMenu();
    AppendMenuW(hPopupMenu, 0, IDM_PLAYLIST_PLAY, L"Воспроизвести");
    AppendMenuW(hPopupMenu, MF_SEPARATOR, -1, L"");
    AppendMenuW(hPopupMenu, MF_POPUP, (UINT_PTR)hSortMenu, L"Упорядочить");
    AppendMenuW(hPopupMenu, MF_SEPARATOR, -1, L"");
    AppendMenuW(hPopupMenu, 0, IDM_PLAYLIST_DELETE, L"Удалить");

    HRSRC hSource = FindResource(hInstance, L"thumbnail", RT_RCDATA);
    HGLOBAL hGlobal = LoadResource(hInstance, hSource);
    LPVOID firstByte = LockResource(hGlobal);
    UINT size = SizeofResource(hInstance, hSource);
    pThumbnail = SHCreateMemStream((const BYTE*)firstByte, size);

    pFontFamily = new FontFamily{ L"Microsoft YaHei" };

    // Message loop
	while (GetMessage(&msg, hWnd, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

    // Freing memory
    SafeRelease(&pThumbnail);

    DestroyMenu(hPopupMenu);

    DeleteBrush(toolBrush);

    pPlayer->DestroyPlayer();
    pPlayList->SavePlaylist();
    pPlayList->DestroyPlaylist();

    GdiplusShutdown(gpStartupToken);

    if (isStarted)
        MFShutdown();

	return msg.wParam;
}

HRESULT RequestFileByIndex(BOOL potential, int index) {
    HRESULT hr = S_OK;
    LPWSTR filePath;
    hr = pPlayList->RequestFileByIndex(potential, index, &filePath);
    if (SUCCEEDED(hr)) {
        hr = pPlayer->OpenFile(filePath);
    }
    filePath = NULL;
    return hr;
}

HRESULT RequestPreviousFile(WPARAM wParam) {
    HRESULT hr;
    LPWSTR filePath = NULL;
    hr = pPlayList->RequestPreviousFile(&filePath);
    if (SUCCEEDED(hr)) {
        hr = pPlayer->OpenFile(filePath);
    }
    filePath = NULL;
    return hr;
}

HRESULT RequestNextFile(WPARAM wParam, BOOL bAuto) {
    HRESULT hr;
    LPWSTR filePath = NULL;
    hr = pPlayList->RequestNextFile(&filePath, bAuto);
    if (SUCCEEDED(hr)) {
        hr = pPlayer->OpenFile(filePath);
    }
    filePath = NULL;
    return hr;
}

HRESULT OnSpace() {
    HRESULT hr;
    if (pPlayer->playerState == paused || pPlayer->playerState == stopped) {
        hr = pPlayer->Play();
        if (SUCCEEDED(hr))
            Controls_EnablePlay();
    }
    else if (pPlayer->playerState == playing) {
        hr = pPlayer->Pause();
        if (SUCCEEDED(hr))
            Controls_DisablePlay();
    }
    else {
        hr = RequestFileByIndex(FALSE, 0);
        if (SUCCEEDED(hr))
            Controls_EnablePlay();
    }
    return hr;
}

LRESULT WINAPI WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    HDC hdc;
    PAINTSTRUCT ps;
    HWND hFrom = NULL;
    POINT cursorPos = {};
    LVHITTESTINFO lvHittestInfo = {};
    LPWSTR filePath = NULL;
    int iItem = 0;

    switch (msg) {
        // Window creating
    case WM_CREATE:
        CreateScreenBuffer(hWnd);
        break;
        // Window size constrains
    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)lParam)->ptMinTrackSize = wndSize;
        break;
        //Window size changed
    case WM_SIZE:
        if (pPlayer)
            pPlayer->SeekBar_OnWindowSizeChanged();
        ResizeScreenBuffer(hWnd);
        if (pPlayList)
            pPlayList->OnWindowSizeChanged();
        ToolBar_OnWindowSizeChanged();
        Controls_OnWindowSizeChanged();
        break;
        // Update window
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);

        RefreshScreenBuffer();
        DrawScreenBuffer(hdc);

        EndPaint(hWnd, &ps);
        break;
    case WM_NOTIFY:
        if (pPlayList) {
            hFrom = pPlayList->RetrieveWindowHandle();
            LPNMLISTVIEW lv = (LPNMLISTVIEW)lParam;
            if (lv->hdr.hwndFrom == hFrom) {
                iItem = lv->iItem;
                switch (lv->hdr.code) {
                case NM_RCLICK:
                    GetCursorPos(&cursorPos);
                    pPlayList->SelectPotentialFile(iItem);
                    TrackPopupMenu(hPopupMenu, 0, cursorPos.x, cursorPos.y, 0, hWnd, NULL);
                    break;
                case NM_DBLCLK:
                    if (iItem >= 0) {
                        RequestFileByIndex(FALSE, iItem);
                        //SetFocus(hWnd);
                    }
                    break;
                case NM_CUSTOMDRAW:
                    return pPlayList->PlaylistCustomDraw((LPNMLVCUSTOMDRAW)lParam);
                }
            }
        }
        break;
    case WM_COMMAND:
        hFrom = pPlayList->RetrieveWindowHandle();
        if (HIWORD(wParam) == 0) {
            switch (LOWORD(wParam)) {
            case IDM_PLAYLIST_PLAY:
                RequestFileByIndex(TRUE, -1);
                //SetFocus(hWnd);
                break;
            case IDM_PLAYLIST_DELETE:
                pPlayList->DeleteFiles();
                break;
            }
            if (LOWORD(wParam) >= SORT_TITLE && LOWORD(wParam) <= SORT_LENGTH_REVERSE)
                pPlayList->Sort(LOWORD(wParam));
        }
        if (LOWORD(wParam) == SORT_TITLE)

        break;
    case WM_KEYDOWN:
        if (wParam == VK_SPACE) {
            OnSpace();
        }
        break;
	case WM_CLOSE:
        ReleaseScreenBuffer();
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
    // Seek bar update
    case WM_TIMER:
        if (wParam == TIMER_SEEKBAR)
            return pPlayer->SeekBar_OnPlaybackTick();
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        pPlayer->SeekBar_OnMouseDown(wParam, lParam);
        ToolBar_OnMouseDown();
        Controls_OnMouseDown();
        break;
    case WM_MOUSEMOVE:
        pPlayer->SeekBar_OnMouseMove(wParam, lParam);
        ToolBar_HitTest(lParam);
        Controls_HitTest(lParam);
        break;
    case WM_LBUTTONUP:
        pPlayer->SeekBar_OnMouseUp();
        ToolBar_OnMouseUp();
        Controls_OnMouseUp();
        ReleaseCapture();
        break;
    // Media events processing
    case WM_APP_PLAYER_EVENT:
        switch (wParam) {
        case W_REQUEST_NEXT_FILE:
            InvalidateRect(hWnd, &infoRect, FALSE);
            //InvalidateRect(hWnd, NULL, FALSE);
            SetWindowText(hWnd, L"Player");
            RequestNextFile(wParam, TRUE);
            break;
        case W_SESSION_STARTED:
            Controls_EnablePlay();
            break;
        case W_SESSION_ENDED:
            Controls_DisablePlay();
            break;
        }
        break;
    // Tool bar events processing
    case WM_APP_TOOLBAR:
        switch (wParam) {
        case TOOL_ADD:
            pPlayList->AddFilesToPlaylist();
            SetFocus(hWnd); //////////////
            break;
        case TOOL_REMOVE:
            pPlayList->DeleteFiles();
            break;
        case TOOL_SORT:
            GetCursorPos(&cursorPos);
            TrackPopupMenu(hSortMenu, 0, cursorPos.x, cursorPos.y, 0, hWnd, NULL);
            break;
        }
        break;
    // Controls events processing
    case WM_APP_CONTROLS:
        switch (wParam) {
        case CONTROLS_SHUFFLE:
            pPlayList->Randomize();
            break;
        case CONTROLS_PREVIOUS:
            if (SUCCEEDED(RequestPreviousFile(wParam)))
                Controls_EnablePlay();
            break;
        case CONTROLS_PAUSE:
            OnSpace();
            break;
        case CONTROLS_NEXT:
            if (SUCCEEDED(RequestNextFile(wParam, FALSE)))
                Controls_EnablePlay();
            break;
        case CONTROLS_REPEAT:
            Controls_SwitchButton(CONTROLS_REPEAT);
            pPlayList->SwitchRepeat();
            break;
        }
        break;
    // Playlist events processing
    case WM_APP_PLAYLIST:
        switch (wParam) {
        case PLAYLIST_NEWFILE:
            SafeRelease(&pImageStream);
            if (pCurrFile) {
                pCurrFile->refCount--;
                if (pCurrFile->refCount == 0) {
                    free(pCurrFile);
                    pCurrFile = NULL;
                }
            }

            pCurrFile = (ListFile*)lParam;
            pCurrFile->refCount++;
            pImageStream = pCurrFile->pThumbnail ? pCurrFile->pThumbnail : pThumbnail;
            pImageStream->AddRef();
            SetWindowTextW(hWnd, pCurrFile->fileName);
            InvalidateRect(hWnd, &infoRect, FALSE);
            break;
        }
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void CreateScreenBuffer(HWND hWnd) {
    ULONG err;
    HDC windowDC;
    if (!(GetClientRect(hWnd, &screenBufferRect)))
        err = GetLastError();
    if (!(windowDC = GetDC(hWnd)))
        err = GetLastError();
    if (!(hdcScreenBuffer = CreateCompatibleDC(windowDC)))
        err = GetLastError();
    if (!(SaveDC(hdcScreenBuffer)))
        err = GetLastError();
    if (!(hScreenBufferBitmap = CreateCompatibleBitmap(windowDC, screenBufferRect.right - screenBufferRect.left,
        screenBufferRect.bottom - screenBufferRect.top)))
        err = GetLastError();
    if (!(SelectObject(hdcScreenBuffer, hScreenBufferBitmap)))
        err = GetLastError();
    if (!(SetBkMode(hdcScreenBuffer, TRANSPARENT)))
        err = GetLastError();
    if (!(ReleaseDC(hWnd, windowDC)))
        err = GetLastError();
}

void ReleaseScreenBuffer() {
    ULONG err;
    if (hdcScreenBuffer) {
        if (!(RestoreDC(hdcScreenBuffer, -1)))
            err = GetLastError();
        if (!(DeleteObject(hScreenBufferBitmap)))
            err = GetLastError();
        if (!(DeleteDC(hdcScreenBuffer)))
            err = GetLastError();
        hdcScreenBuffer = NULL;
    }
}

void ResizeScreenBuffer(HWND hWnd) {
    ReleaseScreenBuffer();
    CreateScreenBuffer(hWnd);
}

void RefreshScreenBuffer() {
    ULONG err;
    RECT rect = { screenBufferRect.left, screenBufferRect.bottom - 100,
                  screenBufferRect.right, screenBufferRect.bottom };
    FillRect(hdcScreenBuffer, &screenBufferRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
    if (err = GetLastError())
        err = 0;
    FillRect(hdcScreenBuffer, &rect, toolBrush);
    if (err = GetLastError())
        err = 0;
    DrawFileInfo(hdcScreenBuffer, pCurrFile);
    if (err = GetLastError())
        err = 0;
    ToolBar_Draw(hdcScreenBuffer);
    if (err = GetLastError())
        err = 0;
    Controls_Draw(hdcScreenBuffer);
    if (err = GetLastError())
        err = 0;
    pPlayer->SeekBar_Repaint(hdcScreenBuffer);
    if (err = GetLastError())
        err = 0;
}

void DrawScreenBuffer(HDC hdc) {
    ULONG err;
    BitBlt(hdc, 0, 0, screenBufferRect.right - screenBufferRect.left, screenBufferRect.bottom - screenBufferRect.top,
        hdcScreenBuffer, 0, 0, SRCCOPY);
    if (err = GetLastError())
        err = 0;
}

void DrawFileInfo(HDC hdc, ListFile* pFile) {
    if (pFile && pPlayer->playerState != ready) {
        Graphics pGraphics(hdc);
        Image image(pImageStream);
        Font titleFont(pFontFamily, 13, FontStyleBold, UnitPoint);
        Font artistFont(pFontFamily, 11, FontStyleRegular, UnitPoint);
        Font albumFont(pFontFamily, 9, FontStyleRegular, UnitPoint);
        RectF rect(30, 170, 200, 45);
        SolidBrush brush(Color(255, 0, 0, 0));
        StringFormat strFormat(0);
        strFormat.SetTrimming(StringTrimmingEllipsisCharacter);

        pGraphics.DrawImage(&image, 30, 30, 130, 130);

        pGraphics.DrawString(pFile->title ? pFile->title : pFile->fileName, -1, &titleFont, rect, &strFormat, &brush);
        rect.Y = rect.Y + rect.Height + 10;
        rect.Height -= 5;
        pGraphics.DrawString(pFile->artist ? pFile->artist : noArtistStr, -1, &artistFont, rect, &strFormat, &brush);
        rect.Y = rect.Y + rect.Height + 10;
        pGraphics.DrawString(pFile->album ? pFile->album : noAlbumStr, -1, &albumFont, rect, &strFormat, &brush);
        //rect.Y = rect.X + rect.Height + 10;
    }
}