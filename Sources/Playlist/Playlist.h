#pragma once

#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <CommCtrl.h>
#include <propkey.h>
#include <Uxtheme.h>
#include <gdiplus.h>
#include <strsafe.h>
#include <time.h>

using namespace Gdiplus;

#define WM_APP_PLAYLIST WM_APP + 10

#define PLAYLIST_NEWFILE 0

#define PLAYLIST_REPEAT_ENABLED     10
#define PLAYLIST_REPEAT_ONE         11
#define PLAYLIST_REPEAT_NONE        12
// for sort codes
#include "../../Resources/resources.h"

//noTitleStr = L"<Нет названия>";
//WCHAR* noArtistStr = (WCHAR*)L"<Неизвестный исполнитель>";
//WCHAR* noAlbumStr = (WCHAR*)L"<Неизвестный альбом>";
//WCHAR* noGenreStr = (WCHAR*)L"<Жанр не указан>";
#define noArtistStr (WCHAR*)L"<Неизвестный исполнитель>"
#define noAlbumStr (WCHAR*)L"<Неизвестный альбом>"
#define noGenreStr (WCHAR*)L"<Жанр не указан>"
//WCHAR noYearStr = (WCHAR)L"<Год выхода не указан>";

typedef struct __listfile {
    //HANDLE handle; // 
    LPWSTR filePath;
    UINT filePathLen; // with '0'
    LPWSTR fileName;
    UINT fileNamePos;

    LPWSTR artist; // artist
    LPWSTR title;  // title
    LPWSTR album;  // album
    LPWSTR genre;  // genre
    MFTIME duration;   // duration
    WCHAR durationStr[11];// duration but string
    IStream* pThumbnail;

    int refCount = 0;
    // duration
} ListFile;

class PlayList {
    static const int MAX_PATH_LENGTH = 65535;
    static const int DEFAULT_CAPACITY = 10;

    HWND hWnd;
    HWND hListView;
    OPENFILENAME openedFile;
    LVITEMW listViewItem;
    IMFSourceResolver* pSourceResolver;
    POINT pos; // x - constant
    SIZE size;
    POINT offset; // x - dist from lower bound, y - dist from right bound

    FontFamily* pFontFamily;
    Font* pFont;
    SolidBrush* pWhiteBrush;
    SolidBrush* pHoverBrush;
    SolidBrush* pSelectedBrush;

    const LPWSTR playlistFile = (LPWSTR)L"playlist.pldata";
    ListFile** pList = NULL; // array of files of playlist
    int* pRandomList = NULL;
    int capacity = 0;
    int listSize = 0;
    int nowPlayingIndex = -1;
    BOOL isCurrFileDeleted = FALSE;
    int potentialIndex = -1;
    BOOL isRepeat;
    BOOL isShuffle;
    

    HRESULT InitPlayList(HINSTANCE hInst, HWND hWnd);

    HRESULT CreatePlaylistFormOpen();
    HRESULT AddToPlaylist(LPWSTR path, size_t fileNamePos, size_t fpLen);

    HRESULT DeleteFromPlaylist(ListFile** pList, int selected);
    HRESULT DeleteOneFile(ListFile** pList, int index);
    HRESULT UpdateView();

    // Buffer support
    HDC hdcItemBuffer;
    HBITMAP hItemBitmap;
    RECT bufferRect;
    void CreateScreenBuffer(SIZE pSize);
    void ReleaseScreenBuffer();
    void ResizeScreenBuffer(RECT* prect);
    void RefreshScreenBuffer(LPNMLVCUSTOMDRAW pDrawInfo);
    void DrawScreenBuffer(HDC hdc);

    void    QuickSort(int l, int r, int filter);
public:
    static PlayList* CreatePlaylist(HINSTANCE hInst, HWND hWnd, LONG x, LONG y);
    void DestroyPlaylist();

    HWND RetrieveWindowHandle();

    HRESULT LoadPlaylist();
    HRESULT SavePlaylist();

    HRESULT AddFilesToPlaylist();
    HRESULT RequestNextFile(LPWSTR* pFilePath, BOOL bAuto);
    HRESULT RequestPreviousFile(LPWSTR* pFilePath);
    HRESULT RequestFileByIndex(BOOL potential, int index, LPWSTR *pFilePath);
    void    SelectPotentialFile(int index);
    HRESULT Sort(int filter);
    HRESULT Randomize();
    HRESULT DeleteFiles();

    // Repeat
    BOOL SwitchRepeat();

    HRESULT OnWindowSizeChanged();
    INT PlaylistCustomDraw(LPNMLVCUSTOMDRAW pDrawInfo);
};