#include "Playlist.h"
#include <string.h>
template <class T> void SafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

PlayList* PlayList::CreatePlaylist(HINSTANCE hInst, HWND hWnd, LONG x, LONG y) {
    PlayList* pPlayList = new PlayList{};
    pPlayList->InitPlayList(hInst, hWnd);

    return pPlayList;
}

void PlayList::DestroyPlaylist() {
    delete this;
}

HRESULT PlayList::InitPlayList(HINSTANCE hInst, HWND hWnd) {
    HRESULT hr;

    // Calc playlist window size
    RECT wndRect;
    GetClientRect(hWnd, &wndRect);
    pos.x = (wndRect.right - wndRect.left) >> 1;
    pos.y = 15;
    offset.x = 20;
    offset.y = 100 + 50;  // toolbar height + offset
    size.cx = pos.x - offset.x;
    size.cy = (wndRect.bottom - wndRect.top) - pos.y - offset.y;

    // Save parent window
    this->hWnd = hWnd;
    // Create window for displaying playlist
    hListView = CreateWindowExW(0,
        WC_LISTVIEW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        pos.x, pos.y, size.cx, size.cy,
        hWnd, NULL, hInst, 0);

    ListView_SetView(hListView, LV_VIEW_TILE);
    ListView_SetExtendedListViewStyle(hListView, LVS_EX_DOUBLEBUFFER);
    SetWindowTheme(hListView, L"Explorer", NULL);
    
    pFontFamily = new FontFamily(L"Microsoft YaHei");
    pFont = new Font(pFontFamily, 9, FontStyleBold, UnitPoint);

    pWhiteBrush = new SolidBrush(Color(255, 255, 255));
    pHoverBrush = new SolidBrush(Color(241, 201, 255)); // light blue
    pSelectedBrush = new SolidBrush(Color(188, 182, 252)); // purple

    // Prepare structure for opening files
    memset(&openedFile, 0, sizeof(OPENFILENAMEW));
    openedFile.lStructSize = sizeof(OPENFILENAMEW);
    openedFile.hwndOwner = hListView;
    openedFile.lpstrFilter = L"MP3 files\0*.mp3\0WAV files\0*.wav\0Media files\0*.mp3;*.wav;*.aac;*.ac3\0";
    openedFile.lpstrFile = (LPWSTR)calloc(MAX_PATH_LENGTH, sizeof(WCHAR));
    openedFile.lpstrFile[0] = 0;
    openedFile.nMaxFile = MAX_PATH_LENGTH;
    openedFile.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_NOCHANGEDIR;
    openedFile.lpstrDefExt = L"";

    // Initialize structure for inserting
    //listViewItem.iItem = 0;
    listViewItem.mask = LVIF_STATE | LVIF_TEXT;
    listViewItem.state = 0;
    //listViewItem.pszText = NULL;
    //listViewItem.cchTextMax = 0;

    LVTILEVIEWINFO lvTile = {};
    lvTile.cbSize = sizeof(LVTILEVIEWINFO);
    lvTile.dwMask = LVTVIM_TILESIZE | LVTVIM_COLUMNS | LVTVIM_LABELMARGIN;
    ListView_GetTileViewInfo(hListView, &lvTile);
    lvTile.dwFlags = LVTVIF_FIXEDSIZE;
    lvTile.sizeTile = { size.cx - 25, 50 };
    CreateScreenBuffer(lvTile.sizeTile);
    ListView_SetTileViewInfo(hListView, &lvTile);

    // Create source resolver for getting file properties
    hr = MFCreateSourceResolver(&pSourceResolver);

    isRepeat = PLAYLIST_REPEAT_ENABLED;
    //isShuffle = FALSE;

    LoadPlaylist();

    return S_OK;
}

/* 
* 
*  *****  Adding files to playlist  *****
* 
*/

HRESULT PlayList::CreatePlaylistFormOpen() {
    HRESULT hr = S_OK;
    int isFail = FALSE;
    LPWSTR filesPath = openedFile.lpstrFile;
    LPWSTR dirStr = NULL;
    size_t dirStrSize = 0;
    LPWSTR currFilePath = NULL;
    size_t fpLen = 0;
    size_t strPos = 0;
    size_t startPos;
    // start waiting
    HCURSOR hCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

    // extract a directory path
    strPos = openedFile.nFileOffset;
    dirStrSize = strPos + 1; // with '0'
    dirStr = (LPWSTR)calloc(dirStrSize, sizeof(WCHAR));
    dirStr[0] = 0;
    filesPath[strPos - 1] = 0;
    wcscat_s(dirStr, dirStrSize, filesPath);

    dirStr[strPos - 1] = '\\';
    dirStr[strPos] = 0;
    dirStrSize--;
    //extract files
    do {
        startPos = strPos;
        do {
            strPos++;
        } while (filesPath[strPos] != 0);

        // create a full path string
        fpLen = dirStrSize + strPos - startPos + 1; // with '0'
        currFilePath = (LPWSTR)calloc(fpLen, sizeof(WCHAR));

        // extract a file name
        currFilePath[0] = 0;
        wcscat_s(currFilePath, fpLen, dirStr);
        wcscat_s(currFilePath, fpLen, &(filesPath[startPos]));

        // Add file to playlist
        hr = AddToPlaylist(currFilePath, dirStrSize, fpLen);
        if (FAILED(hr)) {
            free(currFilePath);
            isFail = TRUE;
        }

        //free(currFilePath); // ОСВОБОЖДАТЬ НЕЛЬЗЯ ЕСЛИ УСПЕХ!!!
        currFilePath = NULL;
        strPos++;
    } while (strPos >= openedFile.nMaxFile || filesPath[strPos] != 0);
    
    if (isFail)
        MessageBoxW(hWnd, L"Некоторые файлы не удалось добавить", L"", MB_ICONERROR);
    UpdateView();
    free(dirStr);
    // end waiting
    SetCursor(hCur);
    return S_OK;
}

/* Get file properties from media source */
HRESULT GetFileProperties(IMFMediaSource* pMediaSource, ListFile* pItem);
/* Get file name from file path and properties */
LPWSTR  GetFileName(ListFile* pListFile, size_t fileNamePos);

void GetStrFromTime(MFTIME time, LPWSTR string);

/* Add file to playlist from file path and PROBABLY needed fie name position */
HRESULT PlayList::AddToPlaylist(LPWSTR filePath, size_t fileNamePos, size_t fpLen) {
    IUnknown* pSource = NULL;
    MF_OBJECT_TYPE objType = {};
    IMFMediaSource* pMediaSource = NULL;
    HRESULT hr;

    if (listSize >= capacity) {
        ListFile** pNewList = (ListFile**)realloc(pList, 
            (size_t)((capacity = capacity != 0 ? listSize + (listSize >> 1) : DEFAULT_CAPACITY) * sizeof(ListFile*)));
        if (pNewList)
            pList = pNewList;
        else
            return E_OUTOFMEMORY;
    }

    // new element
    hr = pSourceResolver->CreateObjectFromURL(filePath, MF_RESOLUTION_MEDIASOURCE |
        MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE, NULL, &objType, (IUnknown**)&pSource);
    if (FAILED(hr))
        return hr;
    hr = pSource->QueryInterface(IID_PPV_ARGS(&pMediaSource));

    pList[listSize] = (ListFile*)malloc(sizeof(ListFile));
    pList[listSize]->refCount = 1;

    hr = GetFileProperties(pMediaSource, pList[listSize]);
    pList[listSize]->filePath = filePath;
    pList[listSize]->fileName = GetFileName(pList[listSize], fileNamePos);
    pList[listSize]->fileNamePos = fileNamePos;
    pList[listSize]->filePathLen = fpLen; // with '0'

    listSize++;

    SafeRelease(&pMediaSource);
    SafeRelease(&pSource);
    return hr;
}

LPWSTR GetStringFromVector(CALPWSTR pv) {
    size_t len = 0;
    LPWSTR str = NULL;
    for (UINT i = 0; i < pv.cElems; i++)
        len += wcslen(pv.pElems[i]) + 2;
    len--;
    str = (LPWSTR)calloc(len, sizeof(WCHAR));
    str[0] = 0;
    for (UINT i = 0; i < pv.cElems; i++) {
        wcscat_s(str, len, pv.pElems[i]);
        if (i != pv.cElems - 1)
            wcscat_s(str, len, L"; ");
    }

    return str;
}

LPWSTR GetFileName(ListFile* pListFile, size_t fileNamePos) {
    LPWSTR str = NULL;
    size_t len, lenArt, lenTitle;

    if (pListFile->artist == NULL)
        lenArt = 0;
    else
        lenArt = wcslen(pListFile->artist);
    if (pListFile->title == NULL)
        lenTitle = 0;
    else
        lenTitle = wcslen(pListFile->title);

    if (lenTitle != 0) {
        len = (lenArt ? lenArt + 3 : 0) + lenTitle + 1;
        str = (LPWSTR)calloc(len, sizeof(WCHAR));
        str[0] = 0;
        if (lenArt) {
            wcscat_s(str, len, pListFile->artist);
            wcscat_s(str, len, L" - ");
        }
        wcscat_s(str, len, pListFile->title);
    } else {
        len = wcslen(&(pListFile->filePath[fileNamePos])) + 1;
        str = (LPWSTR)calloc(len, sizeof(WCHAR));
        str[0] = 0;
        wcscat_s(str, len, &(pListFile->filePath[fileNamePos]));
    }
    return str;
}

void GetStrFromTime(MFTIME time, LPWSTR string) {
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

HRESULT GetFileProperties(IMFMediaSource* pMediaSource, ListFile* pItem) {
    HRESULT hr = S_OK;
    IPropertyStore* pSourceProperties = NULL;
    PROPVARIANT pv;
    size_t currLen = 0;

    hr = MFGetService(pMediaSource, MF_PROPERTY_HANDLER_SERVICE, IID_IPropertyStore, (void**)&pSourceProperties);

    // Artist
    hr = pSourceProperties->GetValue(PKEY_Music_Artist, &pv);
    if (pv.vt == VT_LPWSTR && (currLen = wcslen(pv.pwszVal)) != 0) {
        pItem->artist = (LPWSTR)calloc(++currLen, sizeof(WCHAR));
        pItem->artist[0] = 0;
        wcscat_s(pItem->artist, currLen, pv.pwszVal);
    }
    else if ((pv.vt & VT_VECTOR) && (pv.vt ^ VT_VECTOR) == VT_LPWSTR) {
        pItem->artist = GetStringFromVector(pv.calpwstr);
    }
    else
        pItem->artist = NULL;
    PropVariantClear(&pv);

    // Title
    hr = pSourceProperties->GetValue(PKEY_Title, &pv);
    if (pv.vt == VT_EMPTY || (currLen = wcslen(pv.pwszVal)) == 0)
        pItem->title = NULL;
    else {
        pItem->title = (LPWSTR)calloc(++currLen, sizeof(WCHAR));
        pItem->title[0] = 0;
        wcscat_s(pItem->title, currLen, pv.pwszVal);
    }
    PropVariantClear(&pv);

    // Album
    hr = pSourceProperties->GetValue(PKEY_Music_AlbumTitle, &pv);
    if (pv.vt == VT_EMPTY || (currLen = wcslen(pv.pwszVal)) == 0)
        pItem->album = NULL;
    else {
        pItem->album = (LPWSTR)calloc(++currLen, sizeof(WCHAR));
        pItem->album[0] = 0;
        wcscat_s(pItem->album, currLen, pv.pwszVal);
    }
    PropVariantClear(&pv);

    // Genre
    hr = pSourceProperties->GetValue(PKEY_Music_Genre, &pv);
    if (pv.vt == VT_LPWSTR && (currLen = wcslen(pv.pwszVal)) != 0) {
        pItem->genre = (LPWSTR)calloc(++currLen, sizeof(WCHAR));
        pItem->genre[0] = 0;
        wcscat_s(pItem->genre, currLen, pv.pwszVal);
    }
    else if ((pv.vt & VT_VECTOR) && (pv.vt ^ VT_VECTOR) == VT_LPWSTR) {
        pItem->genre = GetStringFromVector(pv.calpwstr);
    }
    else
        pItem->genre = NULL;
    PropVariantClear(&pv);

    // Duration
    hr = pSourceProperties->GetValue(PKEY_Media_Duration, &pv);
    if (pv.vt != VT_EMPTY)
        pItem->duration = pv.hVal.QuadPart;
    else
        pItem->duration = 0;

    // Tramsform duration to string for output
    GetStrFromTime(pItem->duration, pItem->durationStr);

    // Thumbnail
    hr = pSourceProperties->GetValue(PKEY_ThumbnailStream, &pv);
    if (pv.vt == VT_STREAM) {
        pItem->pThumbnail = pv.pStream;
        pItem->pThumbnail->AddRef();
    } else
        pItem->pThumbnail = NULL;


    PropVariantClear(&pv);
    SafeRelease(&pSourceProperties);
    return S_OK;
}

/*
* 
*     *****  Playlist manage  *****
* 
*/

HWND PlayList::RetrieveWindowHandle() {
    return hListView;
}

HRESULT PlayList::LoadPlaylist() {
    size_t filePathLen = 0;
    size_t fileNamePos = 0;
    LPWSTR currFilePath = NULL;
    HRESULT hr = E_FAIL;
    BOOL isFail = FALSE;
    FILE* plFile = NULL;

    if (_wfopen_s(&plFile, playlistFile, L"rb") == 0) {
        while (fread((void*)&filePathLen, sizeof(ListFile::filePathLen), 1, plFile) != 0) {
            fread((void*)&fileNamePos, sizeof(ListFile::fileNamePos), 1, plFile);
            currFilePath = (LPWSTR)calloc(filePathLen, sizeof(WCHAR));
            currFilePath[0] = 0;
            fread((void*)currFilePath, sizeof(WCHAR), filePathLen, plFile);
            //currFilePath[filePathLen] = 0;
            hr = AddToPlaylist(currFilePath, fileNamePos, filePathLen);
            if (FAILED(hr)) {
                free(currFilePath);
                isFail = TRUE;
            }
            filePathLen = 0;
            fileNamePos = 0;
        }
        fclose(plFile);
        hr = S_OK;
    }
    UpdateView();
    return hr;
}

HRESULT PlayList::SavePlaylist() {
    FILE* plFile = NULL;
    
    if (_wfopen_s(&plFile, playlistFile, L"wb") == 0) {
        for (int i = 0; i < listSize; i++) {
            // 1. file path length
            fwrite((void*)&(pList[i]->filePathLen), sizeof(ListFile::filePathLen), 1, plFile);
            // 2. file name pos
            fwrite((void*)&(pList[i]->fileNamePos), sizeof(ListFile::fileNamePos), 1, plFile);
            // 3. file path
            fputws(pList[i]->filePath, plFile);
            fputwc('\0', plFile);
        }
        fclose(plFile);
        return S_OK;
    }
    return E_FAIL;
}


HRESULT PlayList::AddFilesToPlaylist() {

    if (GetOpenFileNameW(&openedFile)) {
        CreatePlaylistFormOpen();
        // ...
        return S_OK;
    }

    return E_ABORT;
}

HRESULT PlayList::RequestNextFile(LPWSTR *pFilePath, BOOL bAuto) {
    if (listSize == 0)
        return E_BOUNDS;

    if (!(bAuto && isRepeat == PLAYLIST_REPEAT_ONE && !isCurrFileDeleted)) {
        if (!isCurrFileDeleted || nowPlayingIndex == -1)
            nowPlayingIndex++;

        if (nowPlayingIndex >= listSize) {
            if (bAuto && isRepeat == PLAYLIST_REPEAT_NONE)
                return E_ABORT;
            nowPlayingIndex = 0;
        }
    }

    *pFilePath = pList[nowPlayingIndex]->filePath;
    PostMessageW(hWnd, WM_APP_PLAYLIST, PLAYLIST_NEWFILE, (LPARAM)pList[nowPlayingIndex]);
    isCurrFileDeleted = FALSE;
    ListView_RedrawItems(hListView, 0, listSize - 1);
    return S_OK;
}

HRESULT PlayList::RequestPreviousFile(LPWSTR *pFilePath) {
    if (listSize == 0)
        return E_BOUNDS;

    if (nowPlayingIndex <= 0)
        nowPlayingIndex = listSize - 1;
    else
        nowPlayingIndex--;

    *pFilePath = pList[nowPlayingIndex]->filePath;
    PostMessageW(hWnd, WM_APP_PLAYLIST, PLAYLIST_NEWFILE, (LPARAM)pList[nowPlayingIndex]);
    isCurrFileDeleted = FALSE;
    ListView_RedrawItems(hListView, 0, listSize - 1);
    return S_OK;
}

HRESULT PlayList::RequestFileByIndex(BOOL potential, int index, LPWSTR* pFilePath) {
    if (listSize == 0)
        return E_BOUNDS;
    
    if (potential)
        if (potentialIndex == -1)
            return E_ABORT;
        else
            nowPlayingIndex = potentialIndex;
    else
        nowPlayingIndex = index;

    *pFilePath = pList[nowPlayingIndex]->filePath;
    PostMessageW(hWnd, WM_APP_PLAYLIST, PLAYLIST_NEWFILE, (LPARAM)pList[nowPlayingIndex]);
    isCurrFileDeleted = FALSE;
    ListView_RedrawItems(hListView, 0, listSize - 1);
    return S_OK;
}

void PlayList::SelectPotentialFile(int index) {
    potentialIndex = index;
}

HRESULT PlayList::DeleteFiles() {
    int selected = 0;
    LPCWSTR oneStr = (LPWSTR)L"Вы действительно хотите удалить этот файл?";
    LPCWSTR multStr = (LPWSTR)L"Вы действительно хотите удалить выбранные файлы?";
    HRESULT hr = S_OK;
    selected = ListView_GetSelectedCount(hListView);
    if (listSize > 0 && selected > 0 && 
        (MessageBox(hWnd, selected == 1 ? oneStr : multStr, L"Player", MB_ICONWARNING | MB_YESNO) == IDYES)) {
        hr = DeleteFromPlaylist(pList, selected);
    } else
        hr = E_ABORT;
    return hr;
}

HRESULT PlayList::DeleteFromPlaylist(ListFile** pList, int selected) {
    BOOL violate = FALSE;
    UINT itemState = 0;
    int i = 0, iArr = 0;

    for (int iArr = 0; iArr < listSize; ) {
        itemState = ListView_GetItemState(hListView, i, LVIS_SELECTED);
        if (itemState == LVIS_SELECTED) {
            if (iArr < nowPlayingIndex)
                nowPlayingIndex--;
            else if (iArr == nowPlayingIndex)
                isCurrFileDeleted = TRUE;
            DeleteOneFile(pList, iArr);
        }
        else
            iArr++;
        i++;
    }

    if (!listSize/* || nowPlayingIndex >= listSize*/)
        nowPlayingIndex = -1;
    UpdateView();
    return S_OK;
}

HRESULT PlayList::DeleteOneFile(ListFile** pList, int index) {

    pList[index]->refCount--;
    if (pList[index]->refCount == 0) {
        free(pList[index]->filePath);
        //free(pList[index]->fileName);
        if (pList[index]->artist)
            free(pList[index]->artist);
        if (pList[index]->title)
            free(pList[index]->title);
        if (pList[index]->album)
            free(pList[index]->album);
        if (pList[index]->genre)
            free(pList[index]->genre);
        //if (pList[index]->yearStr)
        //    free(pList[index]->yearStr);
        SafeRelease(&(pList[index]->pThumbnail));
    
        free(pList[index]);
        pList[index] = NULL;
    }

    listSize--;
    for (int i = index; i < listSize; i++) {
        pList[i] = pList[i + 1];
    }
    return S_OK;
}

BOOL PlayList::SwitchRepeat() {
    if (isRepeat == PLAYLIST_REPEAT_NONE)
        isRepeat = PLAYLIST_REPEAT_ENABLED;
    else
        isRepeat++;
    return isRepeat;
}

//size_t _wstr_cmp(wchar_t *str1, wchar_t *str2) {
//    wchar_t char1, char2;
//    int pos1, pos2;
//
//    //if ()
//
//    return 0;
//}

INT64 compareTo(ListFile* pListItem1, ListFile* pListItem2, int filter) {
    switch (filter) {
    case SORT_TITLE:
        if (pListItem1->title && pListItem2->title)
            return wcscmp(pListItem1->title, pListItem2->title);
    case SORT_TITLE_REVERSE:
        if (pListItem1->title && pListItem2->title)
            return -wcscmp(pListItem1->title, pListItem2->title);
    case SORT_ARTIST:
        if (pListItem1->artist && pListItem2->artist)
            return wcscmp(pListItem1->artist, pListItem2->artist);
    case SORT_ARTIST_REVERSE:
        if (pListItem1->artist && pListItem2->artist)
            return -wcscmp(pListItem1->artist, pListItem2->artist);
    case SORT_ALBUM:
        if (pListItem1->album && pListItem2->album)
            return wcscmp(pListItem1->album, pListItem2->album);
    case SORT_ALBUM_REVERSE:
        if (pListItem1->album && pListItem2->album)
            return -wcscmp(pListItem1->album, pListItem2->album);
    case SORT_LENGTH:
        return pListItem1->duration - pListItem2->duration;
    case SORT_LENGTH_REVERSE:
        return pListItem2->duration - pListItem1->duration;
    }

    return wcscmp(pListItem1->filePath, pListItem2->filePath);
}

void PlayList::QuickSort(int L, int R, int filter) {
    int l = L, r = R;
    ListFile midElem = *pList[(l + r) >> 1];
    ListFile* swapper = NULL;
    while (l <= r) {
        while (compareTo(pList[l], &midElem, filter) < 0)
            l++;
        while (compareTo(pList[r], &midElem, filter) > 0)
            r--;
        if (l <= r) {
            swapper = pList[l];
            pList[l] = pList[r];
            pList[r] = swapper;
            l++;
            r--;
        }
    }
    if (l < R) QuickSort(l, R, filter);
    if (L < r) QuickSort(L, r, filter);
}

HRESULT PlayList::Sort(int filter) {
    if (listSize == 0)
        return E_BOUNDS;
    ListFile *pSavedFile = NULL;

    if (nowPlayingIndex >= 0 && nowPlayingIndex < listSize && !isCurrFileDeleted)
        pSavedFile = pList[nowPlayingIndex];

    QuickSort(0, listSize - 1, filter);

    if (pSavedFile) {
        for (int i = 0; i < listSize; i++)
            if (pSavedFile == pList[i]) {
                nowPlayingIndex = i;
                break;
            }
    }
    UpdateView();

    return S_OK;
}

HRESULT PlayList::Randomize() {
    int startFrom = 0;
    int randPos = 0;
    if (listSize == 0)
        return E_BOUNDS;
    srand(time(NULL));
    ListFile* swapper = NULL;
    if (nowPlayingIndex >= 0 && nowPlayingIndex < listSize && !isCurrFileDeleted) {
        swapper = pList[0];
        pList[0] = pList[nowPlayingIndex];
        pList[nowPlayingIndex] = swapper;
        nowPlayingIndex = 0;
        startFrom = 1;
    }
    for (startFrom; startFrom < listSize; startFrom++) {
        randPos = (rand() % (listSize - startFrom)) + startFrom;
        swapper = pList[startFrom];
        pList[startFrom] = pList[randPos];
        pList[randPos] = swapper;
    }
    UpdateView();
    return S_OK;
}


/*
*
*      #### Custom draw ####
*
*/

HRESULT PlayList::UpdateView() {
    ListView_DeleteAllItems(hListView);

    for (int i = 0; i < listSize; i++) {
        listViewItem.iItem = listSize;
        listViewItem.pszText = pList[i]->title ? pList[i]->title : pList[i]->fileName;
        listViewItem.cchTextMax = wcslen(pList[i]->fileName) + 1;
        ListView_InsertItem(hListView, &listViewItem);
    }

    return S_OK;
}

HRESULT PlayList::OnWindowSizeChanged() {
    RECT wndRect = {};
    GetClientRect(hWnd, &wndRect);
    pos.x = (wndRect.right - wndRect.left) >> 1;
    pos.y = 15;
    size.cx = pos.x - offset.x;
    size.cy = (wndRect.bottom - wndRect.top) - pos.y - offset.y;

    MoveWindow(hListView, pos.x, pos.y, size.cx, size.cy, TRUE);

    ReleaseScreenBuffer();
    LVTILEVIEWINFO lvTile = {};
    lvTile.cbSize = sizeof(LVTILEVIEWINFO);
    lvTile.dwMask = LVTVIM_TILESIZE | LVTVIM_COLUMNS | LVTVIM_LABELMARGIN;
    ListView_GetTileViewInfo(hListView, &lvTile);
    lvTile.dwFlags = LVTVIF_FIXEDSIZE;
    lvTile.sizeTile = { size.cx - 25, 50 };
    CreateScreenBuffer(lvTile.sizeTile);
    ListView_SetTileViewInfo(hListView, &lvTile);
    return S_OK;
}

void PlayList::CreateScreenBuffer(SIZE pSize) {
    HDC windowDC;
    bufferRect = { 0, 0, pSize.cx, pSize.cy };
    windowDC = GetDC(hListView);
    hdcItemBuffer = CreateCompatibleDC(windowDC);
    SaveDC(hdcItemBuffer);
    hItemBitmap = CreateCompatibleBitmap(windowDC, pSize.cx, pSize.cy);
    SelectObject(hdcItemBuffer, hItemBitmap);
    SetBkMode(hdcItemBuffer, TRANSPARENT);
    ReleaseDC(hListView, windowDC);
}

void PlayList::ReleaseScreenBuffer() {
    if (hdcItemBuffer) {
        RestoreDC(hdcItemBuffer, -1);
        DeleteObject(hItemBitmap);
        DeleteDC(hdcItemBuffer);
        hdcItemBuffer = NULL;
    }
}

void PlayList::RefreshScreenBuffer(LPNMLVCUSTOMDRAW pDrawInfo) {
    Graphics graphics(hdcItemBuffer);
    SolidBrush* pBrush = NULL;
    SolidBrush textBrush(Color(255, 0, 0, 0));
    Font pMainFont(pFontFamily, 9, FontStyleRegular, UnitPoint);
    Font pSecondFont(pFontFamily, 7, FontStyleRegular, UnitPoint);
    Font pSelectedFont(pFontFamily, 9, FontStyleBold, UnitPoint);
    Font* pFont = NULL;
    RectF rect(3, 2, pDrawInfo->nmcd.rc.right - pDrawInfo->nmcd.rc.left - 6, (pDrawInfo->nmcd.rc.bottom - pDrawInfo->nmcd.rc.top) >> 1);
    RECT* pItemRect = &(pDrawInfo->nmcd.rc);
    StringFormat strFormat{};
    POINT cursorPos;
    UINT state = 0;
    int index = pDrawInfo->nmcd.dwItemSpec;
    state = ListView_GetItemState(hListView, index, LVIS_FOCUSED | LVIS_SELECTED);

    GetCursorPos(&cursorPos);
    ScreenToClient(hListView, &cursorPos);

    if (state & LVIS_SELECTED)
        pBrush = pSelectedBrush;
    else if (/*cursorPos.x <= pItemRect->right && cursorPos.y >= pItemRect->top && cursorPos.y <= pItemRect->bottom*/0)
        pBrush = pHoverBrush;
    else
        pBrush = pWhiteBrush;

    if (index == nowPlayingIndex && !isCurrFileDeleted)
        pFont = &pSelectedFont;
    else
        pFont = &pMainFont;

    graphics.FillRectangle(pBrush, 0, 0, (INT)bufferRect.right, (INT)bufferRect.bottom);

    strFormat.SetLineAlignment(StringAlignmentCenter);
    strFormat.SetTrimming(StringTrimmingEllipsisCharacter);
    graphics.DrawString(pList[index]->title ? pList[index]->title : pList[index]->fileName, -1,
        pFont, rect, &strFormat, &textBrush);
    strFormat.SetAlignment(StringAlignmentFar);
    graphics.DrawString(pList[index]->durationStr, -1, pFont, rect, &strFormat, &textBrush);

    rect.Y += rect.Height - 3;
    strFormat.SetAlignment(StringAlignmentNear);
    graphics.DrawString(pList[index]->artist ? pList[index]->artist : noArtistStr, -1, &pSecondFont, rect, &strFormat, &textBrush);
}

INT PlayList::PlaylistCustomDraw(LPNMLVCUSTOMDRAW pDrawInfo) {
    switch (pDrawInfo->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    //case CDDS_ITEMPREERASE:
    case CDDS_ITEMPOSTERASE:
    case CDDS_PREERASE:
    case CDDS_POSTERASE:
        return -1;
    case CDDS_ITEMPREERASE:
    case CDDS_ITEMPREPAINT:
        RefreshScreenBuffer(pDrawInfo);
        BitBlt(pDrawInfo->nmcd.hdc, pDrawInfo->nmcd.rc.left, pDrawInfo->nmcd.rc.top, 
            pDrawInfo->nmcd.rc.right - pDrawInfo->nmcd.rc.left, pDrawInfo->nmcd.rc.bottom - pDrawInfo->nmcd.rc.top,
            hdcItemBuffer, 0, 0, SRCCOPY);
        return CDRF_SKIPDEFAULT;
    }
    return CDRF_DODEFAULT;
}

//INT PlayList::PlaylistCustomDraw(LPNMLVCUSTOMDRAW pDrawInfo) {
//    Graphics graphics(pDrawInfo->nmcd.hdc);
//    SolidBrush* pBrush = NULL;
//    SolidBrush textBrush(Color(255, 0, 0, 0));
//    Font pMainFont(pFontFamily, 9, FontStyleRegular, UnitPoint);
//    Font pSecondFont(pFontFamily, 7, FontStyleRegular, UnitPoint);
//    Font pSelectedFont(pFontFamily, 9, FontStyleBold, UnitPoint);
//    Font* pFont = NULL;
//    RectF rect(pDrawInfo->nmcd.rc.left + 3, pDrawInfo->nmcd.rc.top + 3,
//        pDrawInfo->nmcd.rc.right - pDrawInfo->nmcd.rc.left - 5 - 3, (pDrawInfo->nmcd.rc.bottom - pDrawInfo->nmcd.rc.top - 4) >> 1);
//    StringFormat strFormat{};
//    POINT cursorPos;
//    UINT state = 0;
//    int index = 0;
//
//    switch (pDrawInfo->nmcd.dwDrawStage) {
//    case CDDS_PREPAINT:
//        return CDRF_NOTIFYITEMDRAW;
//    case CDDS_ITEMPREPAINT:
//        index = pDrawInfo->nmcd.dwItemSpec;
//        state = ListView_GetItemState(hListView, index, LVIS_GLOW | LVIS_FOCUSED | LVIS_SELECTED);
//
//        GetCursorPos(&cursorPos);
//        ScreenToClient(hListView, &cursorPos);
//
//        if (state & LVIS_SELECTED)
//            pBrush = pSelectedBrush;
//        else if (/*cursorPos.x <= rect.right && cursorPos.y >= rect.top && cursorPos.y <= rect.bottom*/0)
//            pBrush = pHoverBrush;
//
//        if (index == nowPlayingIndex && !isCurrFileDeleted)
//            pFont = &pSelectedFont;
//        else
//            pFont = &pMainFont;
//
//        if (pBrush)
//            graphics.FillRectangle(pBrush, (INT)pDrawInfo->nmcd.rc.left, (INT)pDrawInfo->nmcd.rc.top,
//                (INT)pDrawInfo->nmcd.rc.right - pDrawInfo->nmcd.rc.left, (INT)pDrawInfo->nmcd.rc.bottom - pDrawInfo->nmcd.rc.top);
//
//        strFormat.SetLineAlignment(StringAlignmentCenter);
//        strFormat.SetTrimming(StringTrimmingEllipsisCharacter);
//        graphics.DrawString(pList[index]->title ? pList[index]->title : pList[index]->fileName, -1, 
//            pFont, rect, &strFormat, &textBrush);
//        strFormat.SetAlignment(StringAlignmentFar);
//        graphics.DrawString(pList[index]->durationStr, -1, pFont, rect, &strFormat, &textBrush);
//        
//        rect.Y += rect.Height - 3;
//        strFormat.SetAlignment(StringAlignmentNear);
//        graphics.DrawString(pList[index]->artist ? pList[index]->artist : noArtistStr, -1, &pSecondFont, rect, &strFormat, &textBrush);
//
//        return CDRF_SKIPDEFAULT;
//    }
//    return CDRF_DODEFAULT;
//}