#pragma once

#include <Windows.h>
#include <shlwapi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mmdeviceapi.h>
#include <propkey.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <strsafe.h>

#define WM_APP_PLAYER_EVENT WM_APP + 100

#define TIMER_SEEKBAR 150

#define W_SESSION_STARTED    1
#define W_SESSION_ENDED      2
#define W_REQUEST_NEXT_FILE  3
#define W_REQUEST_PREV_FILE  4

using namespace Gdiplus;

typedef enum { closed, ready, playing, paused, stopped, error } PlayerState;

class CPlayer : public IMFAsyncCallback {
private:
    IMFMediaSession* pMediaSession;
    //IMFSourceResolver* pSourceResolver;
    IMFMediaSource* pMediaSource;
    IMFPresentationClock* pClock;
    IMFTopology* pTopology;

    BOOL isPMP;

    HWND hWnd;
    HANDLE hClosingEvent;

    ULONG refCount;

    IMMDevice* pPlaybackDevice;
    LPWSTR deviceID;
public:
    PlayerState playerState;
    float volumeLevel;
    float speedLevel;
private:
    // Initialize player, not MF
    CPlayer(HWND hWnd);
    ~CPlayer();

    HRESULT CreateSeekBar(HINSTANCE hInst);
public:
    // Create an instance not by constructor

    // Start MF and player
    static HRESULT CreatePlayer(CPlayer** ppPlayer, HINSTANCE hInst, HWND hWnd);
    // Finish MF and player
    HRESULT DestroyPlayer();

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    // AsyncCallback
    HRESULT STDMETHODCALLTYPE GetParameters(DWORD* pdwFlags, DWORD* pdwQueue);
    HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult* pAsyncResult);

    // Player control
    HRESULT OpenFile(LPWSTR filePath);

    HRESULT Play();
    HRESULT Pause();
    HRESULT Stop();

    // Player properties
    //HRESULT GetFileProperties();

    HRESULT SetVolume(float fLevel);
    HRESULT SetPlaySpeed(float fLevel);
    HRESULT SetPlaybackDevice();  /// to be determined
    HRESULT GetPlaybackDevice(LPWSTR* pwstrDevID);

private:
    HRESULT InitPlayer();

    HRESULT AssignDefaultPlaybackDevice();
   
    HRESULT CloseSession();

// Seek bar support
private:
    POINT pos; // x - constant
    SIZE size;
    POINT offset; // x - dist from lower bound, y - dist from right bound
    LONG timeLen;
    RECT seekBarRect;

    BOOL isActive;
    MFTIME duration, currTime;
    WCHAR durationStr[11];
    WCHAR currTimeStr[11];
    DOUBLE seekPos; // 0 <= seekPos <= 1

    SolidBrush* pBarBrush;
    SolidBrush* pPastBrush;
    SolidBrush* pInactiveBrush;
    BOOL isPointPressed;
    BOOL isSeeked;
    HICON hSeekPointIcon, hSeekPointPressedIcon;
    FontFamily* pFontFamily;
    Font* pFont;
public:
    // Creating and destroying together with player

    // Manage seek bar state
    //HRESULT SeekBar_SetActive();
    //HRESULT SeekBar_SetInactive();

    // Repainting the control in response to events
    HRESULT SeekBar_Repaint(HDC hdc);

    // Event handlers

    HRESULT SeekBar_OnFileOpen();
    HRESULT SeekBar_OnPause();
    HRESULT SeekBar_OnPlaybackTick();
    HRESULT SeekBar_OnPlay();
    HRESULT SeekBar_OnFileEnded();

    HRESULT SeekBar_OnWindowSizeChanged();

    HRESULT SeekBar_OnMouseDown(WPARAM wParam, LPARAM lParam);
    HRESULT SeekBar_OnMouseMove(WPARAM wParam, LPARAM lParam);
    HRESULT SeekBar_OnMouseUp();
    HRESULT SeekBar_OnSeekCancel();

    
};