/* Tell Visual Studio to use C according to the C standard. */
#define _CRT_SECURE_NO_WARNINGS
#include "Windows.h"
#include "commctrl.h"
#define MAX_WPATH (MAX_PATH) /* Windows path limit of 260 wchar_t */

#define VERSION "0.0.1.0"

#include <ctype.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define wcsicmp _wcsicmp

/* Handle for low level hook. */
static HHOOK hHook;


/* Configuration file structure. */
#define MAX_FILE_SIZE_KB 4096

#define FILE_KEY_LEN 27
static const uint8_t FILE_KEY[FILE_KEY_LEN + 1] = "## MouseInput EventMap 01\r\n";


/* Mouse hook procedure */
static LRESULT CALLBACK
LowLevelMouseProc(_In_ int nCode, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	if (nCode < 0) {
		return CallNextHookEx(hHook, nCode, wParam, lParam);
	}

	if (nCode == HC_ACTION) {
		int evCode = (int)wParam;
		MSLLHOOKSTRUCT *pEvent = (MSLLHOOKSTRUCT *)lParam;
		int op = 0;

		static uint64_t	evCount = 0;
		if (evCode == WM_MOUSEMOVE) {
			evCount++;
			op = 1;
		}
		else if (evCode == WM_LBUTTONDOWN) {
			evCount++;
			op = 2;
		}
		else if (evCode == WM_LBUTTONUP) {
			evCount++;
			op = 3;
		}

		if (op) {
			FILE *f = fopen("ev.log", "at");
			if (f) {
				fprintf(f, "[%d] (%i, %i)\n", op, pEvent->pt.x, pEvent->pt.y);
				fclose(f);
			}
		}
	}
	return CallNextHookEx(hHook, nCode, wParam, lParam);
}


/* Tray icon handling procedure */
static const char *AppName = "MouseInput";
#define RC_ICON_ID 100

#define CMD_NONE 199
#define CMD_LABEL 100
#define CMD_EXIT 101
#define CMD_SETTING 102


static void
SetChecked(HWND hWnd, void *_arg)
{
	(void)_arg;
	SendMessage(hWnd, BM_SETCHECK, BST_CHECKED, 0);
}


static LRESULT CALLBACK
TaskbarWndProc(_In_ HWND hWnd, _In_ UINT msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	if (msg == WM_CREATE) {
		hHook = SetWindowsHookExA(WH_MOUSE_LL, LowLevelMouseProc, NULL, 0);
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
			AppendMenu(hMenu, MF_STRING | 0 * MF_GRAYED, CMD_LABEL, AppName);
			AppendMenu(hMenu, MF_SEPARATOR, CMD_NONE, "");
			AppendMenu(hMenu, MF_STRING, CMD_SETTING, "Settings");
			AppendMenu(hMenu, MF_SEPARATOR, CMD_NONE, "");
			AppendMenu(hMenu, MF_STRING, CMD_EXIT, "Exit");
			SetMenuDefaultItem(hMenu, CMD_LABEL, FALSE);
			GetCursorPos(&pt);
			SetForegroundWindow(hWnd);
			TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, hWnd, NULL);
			PostMessage(hWnd, WM_NULL, 0, 0);
			DestroyMenu(hMenu);
		}
	}
	else if (msg == WM_COMMAND) {
		switch (LOWORD(wParam)) {
		case CMD_LABEL: {
			char title[64];
			char info[1024];
			sprintf(title, "%s %s", AppName, VERSION);
			info[0] = 0;
			strcat(info, "TO BE DEFINED");
			MessageBoxA(hWnd, info, title, MB_ICONINFORMATION);
			return 0;
		}
		case CMD_SETTING: {
			MessageBoxA(hWnd, "TO BE DEFINED", AppName, MB_ICONINFORMATION);
			
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


static void ProcessLine(const char * line)
{
	printf("%s", line);
}


/* Map configuration file */
static BOOL
ReadConfigFile()
{
	wchar_t path[MAX_WPATH + 10] = { 0 };
	DWORD len = GetModuleFileNameW(NULL, path, MAX_WPATH);

	if ((len < (MAX_WPATH - 7)) && (len > 4) && (0 == wcsicmp(path + len - 4, L".exe"))) {
		wcscpy(path + len - 4, L".mouse.cfg");
		FILE *f = _wfopen(path, L"rb");
		if (f != NULL) {
			BOOL isValid = TRUE;
			char fileKey[FILE_KEY_LEN] = { 0 };

			size_t bytesRead = fread(fileKey, 1, FILE_KEY_LEN, f);
			if (bytesRead != FILE_KEY_LEN) {
				isValid = FALSE;
			}
			else if (0 != memcmp(fileKey, FILE_KEY, FILE_KEY_LEN)) {
				isValid = FALSE;
			}

			if (!isValid) {
				wchar_t txt[256] = { 0 };
				int dlgRet;
				DWORD bytesWritten = 0;
				BOOL ok = FALSE;

				wcscpy(txt, L"Configuration file invalid:\n");
				wcscat(txt, path);
				wcscat(txt, L"\n\nInitialize config file?");

				dlgRet = MessageBoxW(NULL, txt, L"Error", MB_YESNOCANCEL | MB_ICONHAND);
				if (dlgRet != IDYES) {
					/* Exit program */
					return FALSE;
				}

				f = _wfopen(path, L"w+b");
				if (f) {
					if (fwrite(FILE_KEY, 1, FILE_KEY_LEN, f) == FILE_KEY_LEN) {
						if (0 == fflush(f)) {
							ok = TRUE;
						}
					}
				}

				if (!ok) {
					MessageBoxA(NULL, "Critical error writing config file.", AppName, MB_ICONERROR);
					return FALSE;
				}
			}

			fseek(f, FILE_KEY_LEN, SEEK_SET);

			/* Read File line by line */
			char line[1024] = { 0 };
			while (fgets(line, sizeof(line), f) != NULL) {
				/* Terminate line */
				char *eol = strchr(line, '\r');
				if (eol) *eol = 0;
				eol = strchr(line, '\n');
				if (eol) *eol = 0;
				eol = strchr(line, '#');
				if (eol) *eol = 0;

				/* Tab to space */
				char *t = strchr(line, '\t');
				while (t) {
					*t = ' ';
					t = strchr(line, '\t');
				}

				/* Multiple space to single space */
				char *ss = strstr(line, "  ");
				while (ss) {
					size_t l = strlen(ss);
					memmove(ss, ss + 1, l);
					ss = strstr(line, "  ");
				}

				/* Remove leading space */
				while (line[0] == ' ') {
					size_t l = strlen(line);
					memmove(line, line + 1, l);
				}

				/* Remove trailing space */
				for (;;) {
					size_t l = strlen(line);
					if ((l > 0) && (line[l - 1] == ' ')) {
						line[l - 1] = 0;
					}
					else break;
				}

				/* Ignore empty lines */
				if (!line[0]) {
					continue;
				}

				ProcessLine(line);
			}

			fclose(f);

			return TRUE; // xxx

			MessageBoxA(NULL, "Critical error reading config file.", AppName, MB_ICONERROR);
			return FALSE;
		}

		MessageBoxA(NULL, "Critical error accessing config file.", AppName, MB_ICONERROR);
		return FALSE;
	}

	MessageBoxA(NULL, "Critical error: Invalid executable path.", AppName, MB_ICONERROR);
	return FALSE;
}


int
IsSingleInstance(void)
{
	SECURITY_DESCRIPTOR sd;
	ZeroMemory(&sd, sizeof(sd));
	InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);

	SECURITY_ATTRIBUTES sa;
	ZeroMemory(&sa, sizeof(sa));
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = &sd;
	sa.bInheritHandle = TRUE;

	HANDLE hObj = CreateMutexA(&sa, FALSE, AppName);
	return (WAIT_OBJECT_0 == WaitForSingleObject(hObj, 0));
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

	if (!IsSingleInstance()) {
		return 0;
	}
	if (!ReadConfigFile()) {
		return 1;
	}

	memset(&cls, 0, sizeof(cls));
	cls.lpfnWndProc = TaskbarWndProc;
	cls.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	cls.lpszClassName = AppName;

	RegisterClass(&cls);
	hWnd = CreateWindowA(cls.lpszClassName, AppName, WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
	ShowWindow(hWnd, SW_HIDE);

	hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(RC_ICON_ID), IMAGE_ICON, 16, 16, 0);

	memset(&TrayIcon, 0, sizeof(TrayIcon));
	TrayIcon.cbSize = sizeof(TrayIcon);
	TrayIcon.uID = RC_ICON_ID;
	TrayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	TrayIcon.hIcon = hIcon;
	TrayIcon.hWnd = hWnd;
	// snprintf(TrayIcon.szTip, sizeof(TrayIcon.szTip), "%s", AppName);
	strcpy(TrayIcon.szTip, AppName);
	TrayIcon.uCallbackMessage = WM_USER;
	Shell_NotifyIcon(NIM_ADD, &TrayIcon);

	while (GetMessage(&msg, hWnd, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	Shell_NotifyIcon(NIM_DELETE, &TrayIcon);

	return (int)msg.wParam;
}
