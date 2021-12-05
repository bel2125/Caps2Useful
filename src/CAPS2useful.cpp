/* Caps2Useful for Windows. See "README.md" file for details. */
#include "InputBox.h"
#include "Windows.h"
#define MAX_WPATH (MAX_PATH) /* Windows path limit of 260 wchar_t */

#define VERSION "0.5.0.1"

/* Tell Visual Studio to use C according to the C standard. */
#define _CRT_SECURE_NO_DEPRECATE
#include <malloc.h>
#include <map>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#define snprintf _snprintf
#define vsnprintf _vsnprintf

/* Handle for low level hook. */
static HHOOK hHook;


/* Configuration file structure. */
#define MAX_FILE_SIZE_KB 4096

#define FILE_KEY_LEN 26
static const uint8_t FILE_KEY[FILE_KEY_LEN + 1] = "## Caps2Useful KeyMap 01\r\n";

typedef std::vector<uint8_t> trigger;
typedef struct {
	char actType;
	std::vector<uint8_t> act;
} action;

struct tConfigData {
	uint8_t all_replace;
	std::map<trigger, action> hotkey;
} static config;


/* Status of CapsLock key sequence. */
static BOOL capsIsDown = FALSE;
static BOOL capsKeyValid = FALSE;
#define MAX_KEYLEN 100
static trigger capsKeySeq;

/* https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes */

int8_t
fromHex(char in)
{
	if ((in >= '0') && (in <= '9')) {
		return (in - '0');
	}
	if ((in >= 'A') && (in <= 'F')) {
		return (in - 'A' + 10);
	}
	if ((in >= 'a') && (in <= 'f')) {
		return (in - 'a' + 10);
	}
	return -1;
}


char
toHex(uint8_t in)
{
	char map[] = "0123456789ABCDEF";
	if ((in < 0) || (in > 15)) {
		return 0;
	}
	return map[in];
}


/* Keyboard hook procedure */
static LRESULT CALLBACK
LowLevelKeyboardProc(_In_ int nCode, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	if (nCode < 0) {
		return CallNextHookEx(hHook, nCode, wParam, lParam);
	}

	if (nCode == HC_ACTION) {

		LPKBDLLHOOKSTRUCT pKey = (LPKBDLLHOOKSTRUCT)lParam;
		uint8_t all_replace = config.all_replace;

		if (wParam == WM_KEYDOWN) {
			if (pKey->vkCode == VK_CAPITAL) {
				if (all_replace) {
					keybd_event(all_replace, 0, 0, 0);
					return 1;
				}
				capsIsDown = TRUE;
				capsKeyValid = FALSE;
				capsKeySeq.clear();
				return 1;
			}
			if (capsIsDown) {
				WORD key = LOWORD(pKey->vkCode);
				if (capsKeySeq.empty()) {
					capsKeyValid = TRUE;
				}
				if ((key >= 1) && (key <= 254)) {
					/* Valid range: 1-254 according to
					 * https://docs.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-kbdllhookstruct */
					capsKeySeq.push_back((uint8_t)key);
				} else {
					capsKeyValid = FALSE;
				}
				return 1;
			}
		} else if (wParam == WM_KEYUP) {
			if (pKey->vkCode == VK_CAPITAL) {
				capsIsDown = FALSE;
				if (all_replace) {
					keybd_event(all_replace, 0, KEYEVENTF_KEYUP, 0);
					return 1;
				}
				if (capsKeyValid) {
					action act = config.hotkey[capsKeySeq];
					switch (act.actType) {
					case 'X': {
						char *c = (char *)_alloca(act.act.size() + 1);
						char *p = c;
						for (uint8_t e : act.act) {
							*p = (char)e;
							p++;
						}
						*p = 0;
						ShellExecuteA(NULL, NULL, c, NULL, NULL, SW_NORMAL);
						break;
					}
					case 'K': {
						uint8_t hex = 0;
						int hi = 1;

						for (uint8_t e : act.act) {
							int8_t a = fromHex(e);
							if (a < 0)
								break;

							if (hi) {
								hex = (uint8_t)a << 4;
							} else {
								hex += (uint8_t)a;
								keybd_event(hex, 0, 0, 0);
								keybd_event(hex, 0, KEYEVENTF_KEYUP, 0);
							}
							hi = !hi;
						}
						break;
					}
					}
					capsKeySeq.clear();
					capsKeyValid = FALSE;
				}
				return 1;
			}
		}
	}
	return CallNextHookEx(hHook, nCode, wParam, lParam);
}


/* Tray icon handling procedure */
static const char *CAPS2useful = "CAPS2useful";
#define RC_ICON_ID 100

#define CMD_NONE 199
#define CMD_LABEL 100
#define CMD_EXIT 101
#define CMD_SETTING 102
#define CMD_SEQUENCE 103


static LRESULT CALLBACK
TaskbarWndProc(_In_ HWND hWnd, _In_ UINT msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	if (msg == WM_CREATE) {
		hHook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
	} else if (msg == WM_CLOSE) {
		UnhookWindowsHookEx(hHook);
		PostMessage(hWnd, WM_QUIT, 0, 0);
		return 0;
	} else if (msg == WM_USER) {
		POINT pt;
		HMENU hMenu;
		if (lParam == WM_LBUTTONUP) {
		} else if (lParam == WM_RBUTTONUP) {
			hMenu = CreatePopupMenu();
			AppendMenu(hMenu, MF_STRING | 0 * MF_GRAYED, CMD_LABEL, CAPS2useful);
			AppendMenu(hMenu, MF_SEPARATOR, CMD_NONE, "");
			AppendMenu(hMenu, MF_STRING, CMD_SETTING, "Settings");
			if (config.all_replace == 0) {
				AppendMenu(hMenu, MF_STRING, CMD_SEQUENCE, "Teach Sequence");
			}
			AppendMenu(hMenu, MF_SEPARATOR, CMD_NONE, "");
			AppendMenu(hMenu, MF_STRING, CMD_EXIT, "Exit");
			SetMenuDefaultItem(hMenu, CMD_LABEL, FALSE);
			GetCursorPos(&pt);
			SetForegroundWindow(hWnd);
			TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, hWnd, NULL);
			PostMessage(hWnd, WM_NULL, 0, 0);
			DestroyMenu(hMenu);
		}
	} else if (msg == WM_COMMAND) {
		switch (LOWORD(wParam)) {
		case CMD_LABEL: {
			char title[64];
			char info[1024];
			sprintf(title, "%s %s", CAPS2useful, VERSION);
			info[0] = 0;
			strcat(info, "Turn legacy Caps Lock key into something useful again.\n");
			strcat(info, "\n");
			strcat(info, "Written by me for myself, but free to use for everyone.\n");
			strcat(info, "Home: https://github.com/bel2125/Caps2Useful\n");
			MessageBoxA(hWnd, info, title, MB_ICONINFORMATION);
			return 0;
		}
		case CMD_SETTING: {
			char title[64];
			sprintf(title, "%s Configuration", CAPS2useful);
			struct INPUTBOX IB;
			memset(&IB, 0, sizeof(IB));
			IB.x = 100;
			IB.y = 100;
			IB.width = 300;
			IB.height = 150;
			IB.title = title;
			IB.no_elements = 5;

			IB.element[0].itemtype = "radio";
			IB.element[0].itemtext = "Replace Caps Lock by one useful key:";
			IB.element[0].x = 10;
			IB.element[0].y = 10;
			IB.element[0].width = 280;
			IB.element[0].height = 10;

			IB.element[1].itemtype = "list";
			IB.element[1].itemtext = "";
			IB.element[1].x = 30;
			IB.element[1].y = 25;
			IB.element[1].width = 260;
			IB.element[1].height = 60;

			IB.element[2].itemtype = "radio";
			IB.element[2].itemtext = "Use Caps Lock sequences";
			IB.element[2].x = 10;
			IB.element[2].y = 95;
			IB.element[2].width = 280;
			IB.element[2].height = 10;

			IB.element[3].itemtype = "button";
			IB.element[3].itemtext = "OK";
			IB.element[3].x = 30;
			IB.element[3].y = 110;
			IB.element[3].width = 100;
			IB.element[3].height = 15;
			IB.element[4].itemtype = "button";
			IB.element[4].itemtext = "Cancel";
			IB.element[4].x = 170;
			IB.element[4].y = 110;
			IB.element[4].width = 100;
			IB.element[4].height = 15;

			int r = InputBox(&IB);
			if (r == 2 && IB.result == 1) {
				/* r==2 .. Inputbox closed by button */
				/* IB.result==1 .. Inputbox closed by first button ("OK") */
				if ((IB.value[0].n == 1) && (IB.value[1].n > 0)) {
					config.all_replace = (uint8_t)(IB.value[1].n);
				} else {
					config.all_replace = 0;
				}
			}
			return 0;
		}
		case CMD_EXIT: {
			return TaskbarWndProc(hWnd, WM_CLOSE, 0, 0);
		}
		}
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}


/* Map configuration file */
static BOOL
ReadConfigFile()
{
	wchar_t path[MAX_WPATH + 10] = {0};
	DWORD len = GetModuleFileNameW(NULL, path, MAX_WPATH);
	HANDLE hFile = INVALID_HANDLE_VALUE;

	if ((len < (MAX_WPATH - 3)) && (len > 4) && (0 == wcsicmp(path + len - 4, L".exe"))) {
		wcscpy(path + len - 4, L".keymap");
		hFile = CreateFileW(
		    path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE) {
			LARGE_INTEGER li = {0};
			BOOL isValid = TRUE;
			char fileKey[FILE_KEY_LEN] = {0};

			GetFileSizeEx(hFile, &li);
			SetFilePointer(hFile, 0, 0, FILE_BEGIN);

			if (li.QuadPart <= FILE_KEY_LEN) {
				isValid = FALSE;
			}
			if (isValid) {
				DWORD bytesRead = 0;
				ReadFile(hFile, &fileKey, FILE_KEY_LEN, &bytesRead, NULL);
				if (bytesRead != FILE_KEY_LEN) {
					isValid = FALSE;
				} else if (0 != memcmp(fileKey, FILE_KEY, FILE_KEY_LEN)) {
					isValid = FALSE;
				}
			}

			if (!isValid) {
				wchar_t txt[256] = {0};
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
				if (!WriteFile(hFile, &FILE_KEY, FILE_KEY_LEN, &bytesWritten, NULL)) {
					ok = FALSE;
				}
				if (!FlushFileBuffers(hFile)) {
					ok = FALSE;
				}
				if (bytesWritten != FILE_KEY_LEN) {
					ok = FALSE;
				}
				if (!ok) {
					MessageBoxA(NULL, "Critical error writing config file.", CAPS2useful, MB_ICONERROR);
					return FALSE;
				}
			}

			SetFilePointer(hFile, FILE_KEY_LEN, 0, FILE_BEGIN);

			/* Read File line by line */
			char fileChunk[1024] = {0};
			uint32_t chunkFilled = 0;

			for (;;) {
				/* Fill buffer */
				DWORD bytesRead = 0;
				ReadFile(hFile, &fileChunk, sizeof(fileChunk) - chunkFilled, &bytesRead, NULL);
				chunkFilled += bytesRead;
				if (chunkFilled < sizeof(fileChunk)) {
					ZeroMemory(fileChunk + chunkFilled, sizeof(fileChunk) - chunkFilled);
				}

				/* Process buffer */
				if (0 == memcmp("ALL ", fileChunk, 4)) {
					int8_t hi = fromHex(fileChunk[4]);
					int8_t lo = fromHex(fileChunk[5]);
					if ((hi >= 0) && (lo >= 0)) {
						config.all_replace = ((uint8_t)hi << 4) + (uint8_t)lo;
					}
				} else if (0 == memcmp("S ", fileChunk, 2)) {
					trigger trig;
					int len;
					for (len = 0; len < MAX_KEYLEN; len++) {
						int8_t hi = fromHex(fileChunk[2 * len + 2]);
						int8_t lo = fromHex(fileChunk[2 * len + 3]);
						uint8_t hex = 0;
						if ((hi >= 0) && (lo >= 0)) {
							hex = ((uint8_t)hi << 4) + (uint8_t)lo;
							trig.push_back(hex);
						}
						if (hex == 0)
							break;
					}
					if (len > 0) {
						char *cmd = fileChunk + 2 * len + 2;
						while ((*cmd == ' ') || (*cmd == '\t') || (*cmd == '0'))
							cmd++;
						// if cmd
						action act;
						act.actType = cmd[0];
						cmd++;
						while ((*cmd == ' ') || (*cmd == '\t'))
							cmd++;
						for (;;) {
							uint8_t n = (uint8_t)cmd[0];
							if (n >= ' ')
								act.act.push_back(*cmd);
							else
								break;
							cmd++;
						}
						config.hotkey[trig] = act;
					}
				}

				/* Shift buffer */
				int i = 0;
				while ((i < chunkFilled) && (fileChunk[i] != '\n'))
					i++;
				if (fileChunk[i] != '\n')
					break;
				fileChunk[i] = 0;
				OutputDebugStringA(fileChunk);
				i++; /* character after newline */
				memmove(fileChunk, fileChunk + i, chunkFilled - i);
				chunkFilled -= i;
				ZeroMemory(fileChunk + chunkFilled, sizeof(fileChunk) - chunkFilled);
			}

			CloseHandle(hFile);

			return TRUE; // xxx

			MessageBoxA(NULL, "Critical error reading config file.", CAPS2useful, MB_ICONERROR);
			return FALSE;
		}

		MessageBoxA(NULL, "Critical error accessing config file.", CAPS2useful, MB_ICONERROR);
		return FALSE;
	}

	MessageBoxA(NULL, "Critical error: Invalid executable path.", CAPS2useful, MB_ICONERROR);
	return FALSE;
}


/* Compare file version from source code define and exe properties */
void
GetOwnVersion(char *fileVersionString, char *productVersionString /* >= 24 byte */)
{

	DWORD dwHandle = 0;
	DWORD dwSize;
	wchar_t path[MAX_WPATH + 1] = {0};
	DWORD len = GetModuleFileNameW(NULL, path, MAX_WPATH);
	dwSize = GetFileVersionInfoSizeW(path, &dwHandle);

	if ((dwSize > 0) && (dwSize < 20480)) {
		void *data = _alloca(dwSize);
		if (GetFileVersionInfoW(path, dwHandle, dwSize, data)) {
			UINT len = 0;
			void *buf = NULL;
			if (VerQueryValueW(data, L"\\", &buf, &len)) {
				if (len >= sizeof(VS_FIXEDFILEINFO)) {
					VS_FIXEDFILEINFO *vsFFI = (VS_FIXEDFILEINFO *)buf;
					if (vsFFI->dwSignature == 0xfeef04bdu) {
						sprintf(fileVersionString,
						        "%d.%d.%d.%d",
						        (vsFFI->dwFileVersionMS >> 16) & 0xffffu,
						        (vsFFI->dwFileVersionMS >> 0) & 0xffffu,
						        (vsFFI->dwFileVersionLS >> 16) & 0xffffu,
						        (vsFFI->dwFileVersionLS >> 0) & 0xffffu);
						sprintf(productVersionString,
						        "%d.%d.%d.%d",
						        (vsFFI->dwProductVersionMS >> 16) & 0xffffu,
						        (vsFFI->dwProductVersionMS >> 0) & 0xffffu,
						        (vsFFI->dwProductVersionLS >> 16) & 0xffffu,
						        (vsFFI->dwProductVersionLS >> 0) & 0xffffu);
					}
				}
			}
		}
	}
}


int
CompareVersion(void)
{
	char versionF[32] = {0};
	char versionP[32] = {0};
	GetOwnVersion(versionF, versionP);
	if ((0 != strcmp(versionF, VERSION)) || (0 != strcmp(versionP, VERSION))) {
		MessageBoxA(NULL, "Version mismatch (" VERSION ")", CAPS2useful, MB_ICONERROR);
		return FALSE;
	}
	return TRUE;
}


/* Program entry point. */
int WINAPI
WinMain(_In_ HINSTANCE hInst, _In_ HINSTANCE hPrev, _In_ LPSTR cmdline, _In_ int show)
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


	if (!CompareVersion()) {
		MessageBeep(MB_ICONERROR);
	}

	if (!ReadConfigFile()) {
		return 1;
	}

	memset(&cls, 0, sizeof(cls));
	cls.lpfnWndProc = TaskbarWndProc;
	cls.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	cls.lpszClassName = CAPS2useful;

	RegisterClass(&cls);
	hWnd = CreateWindowA(cls.lpszClassName, CAPS2useful, WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
	ShowWindow(hWnd, SW_HIDE);

	hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(RC_ICON_ID), IMAGE_ICON, 16, 16, 0);

	memset(&TrayIcon, 0, sizeof(TrayIcon));
	TrayIcon.cbSize = sizeof(TrayIcon);
	TrayIcon.uID = RC_ICON_ID;
	TrayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	TrayIcon.hIcon = hIcon;
	TrayIcon.hWnd = hWnd;
	// snprintf(TrayIcon.szTip, sizeof(TrayIcon.szTip), "%s", CAPS2useful);
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
