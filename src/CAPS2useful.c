/* All rights reserved. */
#include "Windows.h"

#define _CRT_SECURE_NO_DEPRECATE
//#include <stdio.h>
#include <stdint.h>
#define snprintf _snprintf
#define vsnprintf _vsnprintf


static HHOOK hHook;
static BOOL capsIsDown = FALSE;

#define MAX_VALUE 4096
#define MAX_WPATH 4096
#define MAX_FILE_SIZE_KB 4096

static const uint32_t FILE_KEY = 0xCAB505F7;


static wchar_t keyName[MAX_VALUE] = { 0 };
static uint32_t keyLen = 0;

struct tFileData {
	uint32_t FILE_KEY;
	uint16_t elem[0x1000];
};

static struct tFileData * pF;


static LRESULT CALLBACK LowLevelKeyboardProc(
	_In_ int nCode,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam)
{
	if (nCode < 0) {
		return CallNextHookEx(hHook, nCode, wParam, lParam);
	}
	if (nCode == HC_ACTION) {
		LPKBDLLHOOKSTRUCT pKey = (LPKBDLLHOOKSTRUCT)lParam;
		if (wParam == WM_KEYDOWN) {
			if (pKey->vkCode == VK_CAPITAL) {
				capsIsDown = TRUE;
				memset(keyName, 0, sizeof(keyName));
				keyLen = 0;
				return 1;
			}
			if (capsIsDown) {
				keyName[keyLen++] = LOWORD(pKey->vkCode);
				return 1;
			}
		}
		else if (wParam == WM_KEYUP) {
			if (pKey->vkCode == VK_CAPITAL) {
				wchar_t value[MAX_VALUE] = { 0 };
				capsIsDown = FALSE;
				DWORD len = 0;// GetPrivateProfileStringW(L"CAPS2useful", keyName, NULL, value, MAX_VALUE, pConfigFileName);
				for (DWORD i = 0; i < len; i++) {
					keybd_event(value[i], 0, 0, 0);
					keybd_event(value[i], 0, KEYEVENTF_KEYUP, 0);
				}
				return 1;
			}
		}
	}
	return CallNextHookEx(hHook, nCode, wParam, lParam);
}


static const char *CAPS2useful = "CAPS2useful";
#define RC_ICON_ID 100

#define CMD_NONE 100
#define CMD_EXIT 101


static LRESULT CALLBACK TaskbarWndProc(
	_In_ HWND hWnd,
	_In_ UINT msg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam)
{
	if (msg == WM_CREATE) {
		hHook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
	}
	else if (msg == WM_CLOSE) {
		UnhookWindowsHookEx(hHook);
		PostMessage(hWnd, WM_QUIT, 0, 0);
		return 0;
	}
	else if (msg == WM_USER) {
		POINT pt;
		HMENU hMenu;
		if (lParam == WM_LBUTTONUP) {
		}
		else if (lParam == WM_RBUTTONUP) {
			hMenu = CreatePopupMenu();
			AppendMenu(hMenu, MF_STRING | MF_GRAYED, CMD_NONE, CAPS2useful);
			AppendMenu(hMenu, MF_SEPARATOR, CMD_NONE, "");
			AppendMenu(hMenu, MF_STRING, CMD_EXIT, "Exit");
			GetCursorPos(&pt);
			SetForegroundWindow(hWnd);
			TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, hWnd, NULL);
			PostMessage(hWnd, WM_NULL, 0, 0);
			DestroyMenu(hMenu);
		}
	}
	else if (msg == WM_COMMAND) {
		switch (LOWORD(wParam)) {
		case CMD_EXIT:
			return TaskbarWndProc(hWnd, WM_CLOSE, 0, 0);
		}
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}


static BOOL ReadConfigFile()
{
	wchar_t path[MAX_WPATH] = { 0 };
	DWORD len = GetModuleFileNameW(NULL, path, MAX_WPATH - 1);
	HANDLE hFile = INVALID_HANDLE_VALUE;

	if ((len < (MAX_WPATH - 3)) && (len > 4) && (0 == wcsicmp(path + len - 4, L".exe"))) {
		wcscpy(path + len - 4, L".map");
		hFile = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE) {
			LARGE_INTEGER li = { 0 };
			BOOL isValid = TRUE;
			uint32_t fileKey = 0;

			GetFileSizeEx(hFile, &li);
			SetFilePointer(hFile, 0, 0, FILE_BEGIN);

			if ((li.QuadPart <= 4) || (li.QuadPart > 1024*MAX_FILE_SIZE_KB)) {
				isValid = FALSE;
			}
			if (isValid) {
				DWORD bytesRead = 0;
				ReadFile(hFile, &fileKey, sizeof(fileKey), &bytesRead, NULL);
				if (bytesRead != sizeof(fileKey)) {
					isValid = FALSE;
				}
				else if (fileKey != FILE_KEY) {
					isValid = FALSE;
				}
			}

			if (!isValid) {
				wchar_t txt[256] = { 0 };
				int dlgRet;
				DWORD bytesWritten = 0;
				BOOL ok = TRUE;

				wcscpy(txt, L"Configuration file invalid:\n");
				wcscat(txt, path);
				wcscat(txt, L"\n\nInitialize config file?");
				
				dlgRet = MessageBoxW(NULL, txt, L"Error", MB_YESNOCANCEL | MB_ICONHAND);
				if (dlgRet != IDYES) {
					/* Exit program */
					return FALSE;
				}
				
				/* Truncate */
				if (SetFilePointer(hFile, 0, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
					ok = FALSE;
				}
				if (!SetEndOfFile(hFile)) {
					ok = FALSE;
				}

				/* Write key */
				fileKey = FILE_KEY;
				if (!WriteFile(hFile, &fileKey, sizeof(fileKey), &bytesWritten, NULL)) {
					ok = FALSE;
				}
				if (!FlushFileBuffers(hFile)) {
					ok = FALSE;
				}
				if (bytesWritten != sizeof(fileKey)) {
					ok = FALSE;
				}
				if (!ok) {
					MessageBoxA(NULL, "Critical error writing config file.", CAPS2useful, MB_ICONERROR);
					return FALSE;
				}
			}

			SetFilePointer(hFile, 0, 0, FILE_BEGIN);

			/* Mapping */
			li.QuadPart = 1024 * MAX_FILE_SIZE_KB;
			HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, li.HighPart, li.LowPart, NULL);
			LPVOID pData = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, li.QuadPart);

			pF = (struct tFileData*)pData;

			/*
			UnmapViewOfFile(pData);
			CloseHandle(hMap);
			CloseHandle(hFile);
			*/
			if (pData != NULL) {
				return TRUE;
			}

			MessageBoxA(NULL, "Critical error mapping config file.", CAPS2useful, MB_ICONERROR);
			return FALSE;
		}

		MessageBoxA(NULL, "Critical error accessing config file.", CAPS2useful, MB_ICONERROR);
		return FALSE;
	}

	MessageBoxA(NULL, "Critical error: Invalid executable path.", CAPS2useful, MB_ICONERROR);
	return FALSE;
}


int WINAPI WinMain(
	_In_ HINSTANCE hInst,
	_In_ HINSTANCE hPrev,
	_In_ LPSTR cmdline,
	_In_ int show)
{
	WNDCLASS cls;
	HWND hWnd;
	HICON hIcon;
	MSG msg;
	NOTIFYICONDATA TrayIcon;

	(void)hInst;
	(void)hPrev;
	(void)cmdline;
	(void)show;

	if (!ReadConfigFile()) {
		return 1;
	}

	memset(&cls, 0, sizeof(cls));
	cls.lpfnWndProc = TaskbarWndProc;
	cls.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	cls.lpszClassName = CAPS2useful;

	RegisterClass(&cls);
	hWnd = CreateWindowA(cls.lpszClassName,
		CAPS2useful,
		WS_OVERLAPPEDWINDOW,
		0,
		0,
		0,
		0,
		NULL,
		NULL,
		NULL,
		NULL);
	ShowWindow(hWnd, SW_HIDE);

	hIcon = (HICON)LoadImage(GetModuleHandle(NULL),
		MAKEINTRESOURCE(RC_ICON_ID),
		IMAGE_ICON,
		16,
		16,
		0);

	memset(&TrayIcon, 0, sizeof(TrayIcon));
	TrayIcon.cbSize = sizeof(TrayIcon);
	TrayIcon.uID = RC_ICON_ID;
	TrayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	TrayIcon.hIcon = hIcon;
	TrayIcon.hWnd = hWnd;
	//snprintf(TrayIcon.szTip, sizeof(TrayIcon.szTip), "%s", CAPS2useful);
	strcpy(TrayIcon.szTip, CAPS2useful);
	TrayIcon.uCallbackMessage = WM_USER;
	Shell_NotifyIcon(NIM_ADD, &TrayIcon);

	while (GetMessage(&msg, hWnd, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	Shell_NotifyIcon(NIM_DELETE, &TrayIcon);

	return (int)msg.wParam;
}
