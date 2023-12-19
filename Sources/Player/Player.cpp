#include <assert.h>

#include "Player.h"

#define CHECK_SUCCESS(hr) if (FAILED(hr)) goto _end;

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

template <class T> void SafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

CPlayer::CPlayer(HWND hWnd) :
    refCount{ 1 },
    hWnd{ hWnd } {
};

CPlayer::~CPlayer() {};

/* Create player instance
* Parameters:
* hInst - handle to resources store instance
* hWnd - handle to player's window
* x, y - seek bar position
*/
HRESULT CPlayer::CreatePlayer(CPlayer** ppPlayer, HINSTANCE hInst, HWND hWnd) {
    CPlayer* pPlayer = new CPlayer{hWnd};
    HRESULT hr;

    // Start media session
    hr = pPlayer->InitPlayer();

    hr = pPlayer->CreateSeekBar(hInst);
    *ppPlayer = pPlayer;
    return hr;
}

HRESULT CPlayer::CreateSeekBar(HINSTANCE hInst) {
    isActive = 0;
    isPointPressed = 0;
    isSeeked = 0;
    seekPos = 0.0;
    hSeekPointIcon = (HICON)LoadIconW(hInst, L"seekPoint");
    hSeekPointPressedIcon = (HICON)LoadIconW(hInst, L"seekPointPressed");
    RECT clRect;
    offset.y = 55;
    offset.x = 70;

    GetClientRect(hWnd, &clRect);
    pos.x = 330;
    pos.y = (clRect.bottom - clRect.top) - offset.y;
    size.cx = (clRect.right - clRect.left) - offset.x - pos.x;
    size.cy = 4;
    timeLen = 50;
    seekBarRect = { pos.x - 15 - timeLen, pos.y - 10, pos.x + size.cx + 15 + timeLen, pos.y + size.cy + 10 };

    pBarBrush = new SolidBrush{ Color(77, 77, 77) };
    pPastBrush = new SolidBrush{ Color(92, 89, 255) };
    pInactiveBrush = new SolidBrush{ Color(77, 77, 77) };
    pFontFamily = new FontFamily{ L"Microsoft YaHei" };
    pFont = new Font{ pFontFamily, 9, FontStyleRegular, UnitPoint };
    return S_OK;
}

HRESULT CPlayer::DestroyPlayer() {
    CloseSession();
    this->Release();
    return 0;
}

// Start Player Session
HRESULT CPlayer::InitPlayer() {
    HRESULT hr;

    pMediaSession = NULL;
    //hr = MFCreateSourceResolver(&pSourceResolver);
    pMediaSource = NULL;
    pClock = NULL;
    pTopology = NULL;

    hr = AssignDefaultPlaybackDevice();
    volumeLevel = 1.0f;
    speedLevel = 1.0f;

    playerState = ready;

    return hr;
}

/* Selects system's default playback device */
HRESULT CPlayer::AssignDefaultPlaybackDevice() {
    HRESULT hr = S_OK;
    IMMDeviceEnumerator* pDevEnumerator = NULL;
    
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, IID_IMMDeviceEnumerator, (void**)&pDevEnumerator);
    hr = pDevEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pPlaybackDevice);
//#ifdef PROJECT_DEBUG
//    CheckProjectForErrors(hr, L"No device found!");
//    IPropertyStore* pDevProperties;
//    PROPVARIANT pv;
//    hr = pPlaybackDevice->OpenPropertyStore(STGM_READ, &pDevProperties);
//    hr = pDevProperties->GetValue(PKEY_Device_FriendlyName, &pv);
//    MessageBox(hWnd, pv.pwszVal, L"Player", 0);
//
//    hr = PropVariantClear(&pv);
//    SafeRelease(&pDevProperties);
//#endif // PROJECT_DEBUG
    SafeRelease(&pDevEnumerator);
    return hr;
}


HRESULT CPlayer::GetPlaybackDevice(LPWSTR *ppwstrDevID) {
    HRESULT hr = E_ACCESSDENIED;
    
    if (pPlaybackDevice) {
        hr = pPlaybackDevice->GetId(ppwstrDevID);
    }
    return hr;
}

HRESULT CPlayer::CloseSession() {
    HRESULT hr = S_OK;
    if (pMediaSession) {
        SafeRelease(&pTopology);
        SafeRelease(&pClock);
        hr = pMediaSession->Close();
        DWORD dwResult = WaitForSingleObject(hClosingEvent, 5000);
        if (dwResult == WAIT_TIMEOUT)
            assert(FALSE);

        CloseHandle(hClosingEvent);
        hr = pMediaSource->Shutdown();
        hr = pMediaSession->Shutdown();
        SafeRelease(&pMediaSource);
        SafeRelease(&pMediaSession);
        SeekBar_OnFileEnded();
        playerState = ready;
    }

    return hr;
}

// IUnknown
HRESULT CPlayer::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {
        QITABENT(CPlayer, IMFAsyncCallback),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

ULONG CPlayer::AddRef() {
    return InterlockedIncrement(&refCount);
}

ULONG CPlayer::Release() {
    ULONG uCount = InterlockedDecrement(&refCount);
    if (uCount == 0) {
        delete this;
    }
    return uCount;
}

// IAsyncCallback
HRESULT CPlayer::GetParameters(DWORD* pdwFlags, DWORD* pdwQueue) {
    return E_NOTIMPL;
}

HRESULT CPlayer::Invoke(IMFAsyncResult* pAsyncResult) {
    HRESULT hr;
    DWORD id;
    MediaEventType eventType = MEUnknown;
    IMFMediaEvent* pEvent = NULL;
    HRESULT status = NULL;
    PROPVARIANT pvValue;
    PropVariantInit(&pvValue);
    // Get an event info
    hr = pMediaSession->EndGetEvent(pAsyncResult, &pEvent);
    hr = pEvent->GetStatus(&status);

    if (SUCCEEDED(status)) {
        hr = pEvent->GetType(&eventType);

        switch (eventType) {
        case MESessionTopologySet:
            hr = pEvent->GetValue(&pvValue);
            if (pvValue.vt == VT_UNKNOWN)
                hr = (pvValue.punkVal)->QueryInterface(&pTopology);
            else
                hr = E_UNEXPECTED;
            break;
        case MESessionTopologyStatus:
            UINT32 topoStatus;
            hr = pEvent->GetUINT32(MF_EVENT_TOPOLOGY_STATUS, &topoStatus);
            if (SUCCEEDED(hr) && topoStatus == MF_TOPOSTATUS_READY)
                SeekBar_OnFileOpen();
            break;
        case MENewPresentation:
            break;
        case MEEndOfPresentation:
        case MEEndOfPresentationSegment:
            playerState = stopped; /// ???
            break;
        case MESessionStarted:
            PostMessage(hWnd, WM_APP_PLAYER_EVENT, W_SESSION_STARTED, 0L);
            break;
        case MESessionStopped:
            playerState = stopped;
        case MESessionEnded:
            playerState = ready;
            SeekBar_OnFileEnded();
            CloseSession(); // ????????
            PostMessage(hWnd, WM_APP_PLAYER_EVENT, W_SESSION_ENDED, 0L);
            PostMessage(hWnd, WM_APP_PLAYER_EVENT, W_REQUEST_NEXT_FILE, 0L);
            break;
        case MESessionClosed:
            SetEvent(hClosingEvent); ///// ???????????
            break;
        default:
            break;
        }
        if (pMediaSession)
            hr = pMediaSession->BeginGetEvent(this, NULL);
    } else {
        return E_NOINTERFACE;
    }

    PropVariantClear(&pvValue);
    return hr;
}

void GetStringFromTime(MFTIME time, LPWSTR string);

HRESULT CreatePlaybackTopology(
    IMFMediaSource* pSource,
    LPWSTR wstrDevID,
    IMFPresentationDescriptor* pPD,
    IMFTopology** ppTopology);

// Opens a file or returns error if media type is not supported
HRESULT CPlayer::OpenFile(LPWSTR filePath) {
    HRESULT hr = S_OK;
    MF_OBJECT_TYPE objType = {};
    IMFSourceResolver* pSourceResolver = NULL;
    IUnknown* pSource = NULL;
    LPWSTR wstrDevID = NULL;
    IMFPresentationDescriptor* pPD = NULL;
    IMFTopology* pTopology = NULL;
    IMFClock* pClockProvider = NULL;

    // Get media source
    hr = MFCreateSourceResolver(&pSourceResolver);
    hr = pSourceResolver->CreateObjectFromURL(filePath, MF_RESOLUTION_MEDIASOURCE |
        MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE, NULL, &objType, (IUnknown**)&pSource);
    if (FAILED(hr))
        return hr;

    if (pMediaSession) {
        hr = CloseSession();
    }
    hr = MFCreateMediaSession(NULL, &pMediaSession);
    hr = pMediaSession->BeginGetEvent(this, NULL);

    hr = pSource->QueryInterface(IID_PPV_ARGS(&pMediaSource));

    // Set up playback device
    hr = GetPlaybackDevice(&wstrDevID);

    // Create the presentation descriptor for the media source
    hr = pMediaSource->CreatePresentationDescriptor(&pPD);

    // Create a partial topology
    hr = CreatePlaybackTopology(pMediaSource, wstrDevID, pPD, &pTopology);

    // Add topology to media session
    hr = pMediaSession->SetTopology(0, pTopology);

    // Get a presentation clock
    hr = pMediaSession->GetClock(&pClockProvider);
    hr = pClockProvider->QueryInterface(IID_IMFPresentationClock, (void**)&pClock);

    if (SUCCEEDED(hr)) {
        hr = pPD->GetUINT64(MF_PD_DURATION, (UINT64*)&duration);
        GetStringFromTime(duration, durationStr);
        //playerState = playing;
        // Play file
        Play();
    }

    SafeRelease(&pClockProvider);
    SafeRelease(&pTopology);
    SafeRelease(&pSource);
    SafeRelease(&pPD);
    SafeRelease(&pSourceResolver);
    return hr;
}

// #### Set up a source for playing

HRESULT AddSourceNode(IMFTopology* pTopology,
    IMFMediaSource* pMediaSource,
    IMFPresentationDescriptor* pPD,
    IMFStreamDescriptor* pSD,
    IMFTopologyNode** ppSourceNode);

HRESULT AddOutputNode(IMFTopology* pTopology, IMFActivate* pOutputActivate, IMFTopologyNode** ppOutputNode);

/*
Creates a partial topology for opened file
Parameters:
* pSource - Media Source
* wstrDevID - Playback device ID string
* pPD - Presentation descriptor.
* ppTopology - Receives a pointer to the topology.
*/
HRESULT CreatePlaybackTopology(
    IMFMediaSource* pSource,
    LPWSTR wstrDevID,
    IMFPresentationDescriptor* pPD,
    IMFTopology** ppTopology)
{
    IMFTopology* pTopology;
    HRESULT hr = S_OK;
 
    DWORD descCount = 0;
    IMFActivate* pSinkActivate = NULL;
    IMFTopologyNode* pSourceNode = NULL;
    IMFTopologyNode* pOutputNode = NULL;
    IMFStreamDescriptor* pSD = NULL;
    BOOL isSelected = 0;

    // Create a media sink activation object with specified device ID
    hr = MFCreateAudioRendererActivate(&pSinkActivate);
    //hr = pSinkActivate->SetString(MF_AUDIO_RENDERER_ATTRIBUTE_ENDPOINT_ID, wstrDevID);

    // Create an empty topology
    hr = MFCreateTopology(&pTopology);

    hr = pPD->GetStreamDescriptorCount(&descCount);

    for (DWORD i = 0; i < descCount; i++) {
        
        // Adding a source node
        hr = pPD->GetStreamDescriptorByIndex(i, &isSelected, &pSD);

        if (isSelected) {
            // Create source node
            hr = AddSourceNode(pTopology, pSource, pPD, pSD, &pSourceNode);

            // Create output node (activate)
            hr = AddOutputNode(pTopology, pSinkActivate, &pOutputNode);

            hr = pSourceNode->ConnectOutput(0, pOutputNode, 0);
        }
        // else not add to topology
        
        SafeRelease(&pSD);
    }

    (*ppTopology) = pTopology;
    (*ppTopology)->AddRef();
    SafeRelease(&pTopology);
    SafeRelease(&pSinkActivate);
    SafeRelease(&pSourceNode);
    SafeRelease(&pOutputNode);
    return hr;
}

/*
Adds an input node to a topology without connection
Parameters:
* pTopology - Topology to be updated
* pMediaSource - Media source
* pPD - Presentation Descriptor
* pSD - Stream Descriptor
* ppSourceNode - pointer to new (source) node
*/
HRESULT AddSourceNode(IMFTopology* pTopology,
                      IMFMediaSource* pMediaSource,
                      IMFPresentationDescriptor *pPD,
                      IMFStreamDescriptor* pSD,
                      IMFTopologyNode** ppSourceNode)
{
    HRESULT hr = S_OK;
    IMFTopologyNode* pSourceNode = NULL;
    
    hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pSourceNode);

    hr = pSourceNode->SetUnknown(MF_TOPONODE_SOURCE, pMediaSource);
    hr = pSourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD);
    hr = pSourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSD);

    // Add the node to the topology.
    hr = pTopology->AddNode(pSourceNode);

    (*ppSourceNode) = pSourceNode;
    (*ppSourceNode)->AddRef();
    SafeRelease(&pSourceNode);
    return hr;
}

/*
Adds an output node to a topology without connection
Parameters:
* pTopology - Topology to be updated
* pOutputActivate - Output source (activate object)
* ppOutputNode - pointer to new (output) node
*/
HRESULT AddOutputNode(IMFTopology* pTopology, IMFActivate* pOutputActivate, IMFTopologyNode** ppOutputNode) {
    HRESULT hr = S_OK;
    IMFTopologyNode* pOutputNode = NULL;

    hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pOutputNode);

    hr = pOutputNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE);
    hr = pOutputNode->SetObject(pOutputActivate);
    hr = pTopology->AddNode(pOutputNode);

    *ppOutputNode = pOutputNode;
    (*ppOutputNode)->AddRef();
    SafeRelease(&pOutputNode);
    return hr;
}



// ### Player controls, seek bar and state management ###

// Resumes/starts playback
HRESULT CPlayer::Play() {
    HRESULT hr = S_OK;
    PROPVARIANT pv;
    PropVariantInit(&pv);

    // ÅÑËÈ ÔÀÉË ÇÀÊÎÍ×ÈËÑß, ÌÛ ÌÎÆÅÌ ÇÀÏÓÑÒÈÒÜ ÒÀÉÌÅÐ ÎÒÑÞÄÀ!!!!
    // ãðàìîòíî çàäàòü ñîñòîÿíèÿ 
    // åñëè ôàéë çàêîí÷èëñÿ è ìû åãî õîòèì îòêðûòü ñíîâà, íàäî ÷åðåç îòêðûòèå
    // (êîíåö ôàéëà - end of presentation, ÍÎ ÍÅ stopped!!)
    //
    if (isSeeked) {
        //MFTIME seekTime = duration * seekPos;
        pv.vt = VT_I8;
        pv.hVal.QuadPart = /*seekTime*/currTime;
        isSeeked = FALSE;
    }
    hr = pMediaSession->Start(&GUID_NULL, &pv);
    SeekBar_OnPlay();

    playerState = playing;
    PropVariantClear(&pv);
    return hr;
}

// Pauses playback
HRESULT CPlayer::Pause() {
    HRESULT hr = S_OK;
    hr = pMediaSession->Pause();
    SeekBar_OnPause();

    playerState = paused;
    return hr;
}

// Stops playback
HRESULT CPlayer::Stop() {
    HRESULT hr = S_OK;
    hr = pMediaSession->Stop();
    
    // Deactivate seek bar
    SeekBar_OnFileEnded();
    playerState = stopped;
    return S_OK;
}

// Player properties

HRESULT CPlayer::SetVolume(float fLevel) {
    return E_NOTIMPL;
}

HRESULT CPlayer::SetPlaySpeed(float fLevel) {
    return E_NOTIMPL;
}

HRESULT CPlayer::SetPlaybackDevice() {
    return E_NOTIMPL;
}

// Manage seek bar

// Repainting the seek bar in response to events
HRESULT CPlayer::SeekBar_Repaint(HDC hdc) {
    HDC thdc = NULL;
    Graphics pGraphics{ hdc };
    //Pen pen{ Color(255, 0, 0, 0) };
    //pGraphics.DrawRectangle(&pen, (INT)seekBarRect.left, (INT)seekBarRect.top,
    //    (INT)seekBarRect.right - seekBarRect.left - 1, (INT)seekBarRect.bottom - seekBarRect.top - 1);
    if (isActive) {
        LONG pointPos = size.cx * seekPos;
        // Past part
        pGraphics.FillRectangle(pPastBrush, (INT)pos.x, (INT)pos.y, (INT)pointPos, (INT)size.cy);
        // Remaining part
        pGraphics.FillRectangle(pBarBrush, (INT)pos.x + pointPos, (INT)pos.y, (INT)size.cx - pointPos, (INT)size.cy);
        thdc = pGraphics.GetHDC();
        if (!isPointPressed)
            DrawIcon(thdc, pos.x + pointPos - 16, pos.y - 14, hSeekPointIcon);
        else
            DrawIcon(thdc, pos.x + pointPos - 16, pos.y - 14, hSeekPointPressedIcon);
        pGraphics.ReleaseHDC(thdc);
        // Duration
        RectF rect(seekBarRect.left, seekBarRect.top, timeLen, seekBarRect.bottom - seekBarRect.top);
        StringFormat strFormat{};
        strFormat.SetLineAlignment(StringAlignmentCenter);
        strFormat.SetAlignment(StringAlignmentFar);
        SolidBrush brush(Color(255, 0, 0, 0));

        pGraphics.DrawString(currTimeStr, -1, pFont, rect, &strFormat, &brush);
        // Current time
        rect.X = seekBarRect.right - timeLen;
        strFormat.SetAlignment(StringAlignmentNear);
        pGraphics.DrawString(durationStr, -1, pFont, rect, &strFormat, &brush);
    }
    else {
        pGraphics.FillRectangle(pInactiveBrush, (INT)pos.x, (INT)pos.y, (INT)size.cx, (INT)size.cy);
    }
    return S_OK;
}

void GetStringFromTime(MFTIME time, LPWSTR string) {
    ULONG timeInSec = time / 10'000'000;
    UINT hours = timeInSec / 3600;
    timeInSec %= 3600;
    UINT minutes = timeInSec / 60;
    timeInSec %= 60;
    UINT seconds = timeInSec;
    if (hours != 0)
        StringCbPrintf(string, 11 * sizeof(WCHAR), L"%d:%d:%.2d", hours, minutes, seconds);
    else
        StringCbPrintf(string, 11 * sizeof(WCHAR), L"%d:%.2d", minutes, seconds);
}

HRESULT CPlayer::SeekBar_OnFileOpen() {
    seekPos = 0.0;
    isSeeked = FALSE;
    isActive = TRUE;
    currTime = 0;
    GetStringFromTime(currTime, currTimeStr);
    InvalidateRect(hWnd, &seekBarRect, FALSE);
    return S_OK;
}

HRESULT CPlayer::SeekBar_OnPause() {
    KillTimer(hWnd, TIMER_SEEKBAR);
    if (!isPointPressed)
        SeekBar_OnPlaybackTick();
    return S_OK;
}

// Timer procedure
HRESULT CPlayer::SeekBar_OnPlaybackTick() {
    HRESULT hr;
    hr = pClock->GetTime(&currTime);
    seekPos = ((DOUBLE)currTime / duration);
    GetStringFromTime(currTime, currTimeStr);
    InvalidateRect(hWnd, &seekBarRect, FALSE);
    return S_OK;
}

HRESULT CPlayer::SeekBar_OnPlay() {
    if (!isPointPressed)
        SetTimer(hWnd, TIMER_SEEKBAR, 250, NULL);
    return S_OK;
}

HRESULT CPlayer::SeekBar_OnFileEnded() {
    KillTimer(hWnd, TIMER_SEEKBAR);
    isActive = FALSE;
    isSeeked = FALSE;
    seekPos = 0.0;
    currTime = 0;
    InvalidateRect(hWnd, &seekBarRect, FALSE);
    UpdateWindow(hWnd);
    return S_OK;
}

HRESULT CPlayer::SeekBar_OnWindowSizeChanged() {
    RECT clRect;
    GetClientRect(hWnd, &clRect);
    //pos.x
    pos.y = (clRect.bottom - clRect.top) - offset.y;
    size.cx = (clRect.right - clRect.left) - offset.x - pos.x;
    //size.cy
    seekBarRect = { pos.x - 10 - timeLen, pos.y - 10, pos.x + size.cx + 10 + timeLen, pos.y + size.cy + 10 };
    return S_OK;
}

HRESULT CPlayer::SeekBar_OnMouseDown(WPARAM wParam, LPARAM lParam) {
    if (isActive && !isPointPressed /*&& ((wParam & -1l) == (WPARAM)MK_LBUTTON)*/) {
        POINT curPos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (curPos.x >= pos.x && curPos.x <= pos.x + size.cx && curPos.y >= pos.y - 7 && curPos.y <= pos.y + size.cy + 7) {
            KillTimer(hWnd, TIMER_SEEKBAR);
            isPointPressed = TRUE;
            seekPos = (DOUBLE)(curPos.x - pos.x) / size.cx;
            currTime = seekPos * duration;
            GetStringFromTime(currTime, currTimeStr);
            SetCapture(hWnd);
            InvalidateRect(hWnd, &seekBarRect, FALSE);
        }
    }
    return S_OK;
}

HRESULT CPlayer::SeekBar_OnMouseMove(WPARAM wParam, LPARAM lParam) {
    if (isActive && isPointPressed) {
        POINT curPos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        seekPos = (DOUBLE)(curPos.x - pos.x) / size.cx;
        if (seekPos < 0.0001)
            seekPos = 0.0;
        else if (seekPos > 0.9999)
            seekPos = 1.0;
        currTime = seekPos * duration;
        GetStringFromTime(currTime, currTimeStr);
        InvalidateRect(hWnd, &seekBarRect, FALSE);
    }
    return S_OK;
}

HRESULT CPlayer::SeekBar_OnMouseUp() {
    if (isActive && isPointPressed) {
        PROPVARIANT pv;
        isPointPressed = FALSE;
        //MFTIME seekTime = duration * seekPos;
        PropVariantInit(&pv);
        pv.vt = VT_I8;
        pv.hVal.QuadPart = /*seekTime*/currTime;
        if (playerState == playing) {
            pMediaSession->Start(&GUID_NULL, &pv);
            SetTimer(hWnd, TIMER_SEEKBAR, 250, NULL);
        } else {
            isSeeked = TRUE;
        }
        InvalidateRect(hWnd, &seekBarRect, FALSE);
    }
    return S_OK;
}

HRESULT CPlayer::SeekBar_OnSeekCancel()
{
    return E_NOTIMPL;
}
