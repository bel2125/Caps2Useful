/* Caps2Useful for Windows. See "README.md" file for details. */
#include "InputBox.h"
#include "KeyList.h"
#include "Windows.h"
#include "commctrl.h"
#define MAX_WPATH (MAX_PATH) /* Windows path limit of 260 wchar_t */

#define VERSION "0.5.1.0"

/* Tell Visual Studio to use C according to the C standard. */
#define _CRT_SECURE_NO_DEPRECATE
#include <ctype.h>
#include <malloc.h>
#include <map>
#include <stdint.h>
#include <stdio.h>
#include <string>
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
						int step = 0;
						std::vector<uint8_t> toRelease;

						for (uint8_t e : act.act) {
							int8_t a = fromHex(e);
							if ((a < 0) && (step < 2)) {
								break;
							}

							if (step == 0) {
								hex = (uint8_t)a << 4;
								step++;
							} else if (step == 1) {
								hex += (uint8_t)a;
								step++;
							} else {
								keybd_event(hex, 0, 0, 0);
								if (e == '+') {
									toRelease.push_back(hex);
								} else {
									keybd_event(hex, 0, KEYEVENTF_KEYUP, 0);
								}
								if (e == '-') {
									for (uint8_t k : toRelease) {
										keybd_event(k, 0, KEYEVENTF_KEYUP, 0);
									}
									toRelease.clear();
								}
								step = 0;
							}
						}

						for (uint8_t k : toRelease) {
							keybd_event(k, 0, KEYEVENTF_KEYUP, 0);
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


static void
SetChecked(HWND hWnd, void *_arg)
{
	(void)_arg;
	SendMessage(hWnd, BM_SETCHECK, BST_CHECKED, 0);
}


static void
PopulateKeyList(HWND hListView, void *sel)
{
	LVCOLUMN lvColumn;
	ZeroMemory(&lvColumn, sizeof(lvColumn));
	lvColumn.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.cx = 50;
	lvColumn.pszText = ("ID");
	ListView_InsertColumn(hListView, 0, &lvColumn);
	lvColumn.cx = 80;
	lvColumn.pszText = ("Name");
	ListView_InsertColumn(hListView, 1, &lvColumn);
	lvColumn.cx = 250;
	lvColumn.pszText = ("Description");
	ListView_InsertColumn(hListView, 2, &lvColumn);

	int setSel = -1;
	uint8_t selId = *(uint8_t *)sel;
	char str[256];

	for (int i = 0; vkey_names[i].name; i++) {
		sprintf(str, "0x%02X ", vkey_names[i].id);
		LVITEMA item;
		ZeroMemory(&item, sizeof(item));
		item.mask = LVIF_TEXT | LVIF_PARAM;
		item.pszText = str;
		item.iItem = i;
		item.lParam = (LPARAM)vkey_names[i].id;
		int pos = ListView_InsertItem(hListView, &item);

		if (vkey_names[i].id == selId) {
			setSel = pos;
		}

		item.mask = LVIF_TEXT;
		item.iItem = i;
		item.iSubItem = 1;
		sprintf(str, "%s", vkey_names[i].name);
		ListView_SetItem(hListView, &item);

		item.iSubItem = 2;
		sprintf(str, "%s", vkey_names[i].descr);
		ListView_SetItem(hListView, &item);
	}

	ListView_SetSelectionMark(hListView, setSel);
	ListView_SetItemState(hListView, setSel, LVNI_SELECTED | LVNI_FOCUSED, LVNI_SELECTED | LVNI_FOCUSED);
}


static void
PopulateHotkeyList(HWND hListView, void *sel)
{
	LVCOLUMN lvColumn;
	ZeroMemory(&lvColumn, sizeof(lvColumn));
	lvColumn.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.cx = 150;
	lvColumn.pszText = ("Triger");
	ListView_InsertColumn(hListView, 0, &lvColumn);
	lvColumn.cx = 65;
	lvColumn.pszText = ("Action");
	ListView_InsertColumn(hListView, 1, &lvColumn);
	lvColumn.cx = 360;
	lvColumn.pszText = ("Action Detail");
	ListView_InsertColumn(hListView, 2, &lvColumn);

	int i = 0;

	for (auto n : config.hotkey) {
		std::string trig_key;
		std::string act_type;
		std::string act_details;

		for (uint8_t a : n.first) {
			trig_key += GetKeyName(a);
		}

		if (n.second.actType == 'X') {
			act_type = "execute";
			for (uint8_t e : n.second.act) {
				act_details += (char)e;
			}
		} else if (n.second.actType == 'K') {
			act_type = "keys";
			int step = 0;
			int hex = 0;
			for (uint8_t e : n.second.act) {
				if (step == 0) {
					hex = (uint8_t)fromHex(e) << 4;
					step++;
				} else if (step == 1) {
					hex += (uint8_t)fromHex(e);
					step++;
				} else {
					act_details += GetKeyName(hex);
					act_details += (char)e;
					step = 0;
				}
			}
		}

		/* Add Item to ListView */
		LVITEMA item;
		ZeroMemory(&item, sizeof(item));
		item.mask = LVIF_TEXT | LVIF_PARAM;
		item.pszText = (char *)trig_key.c_str();
		item.iItem = i;
		item.lParam = i;
		int pos = ListView_InsertItem(hListView, &item);

		item.mask = LVIF_TEXT;
		item.iItem = i;
		item.iSubItem = 1;
		item.pszText = (char *)act_type.c_str();
		ListView_SetItem(hListView, &item);

		item.iSubItem = 2;
		item.pszText = (char *)act_details.c_str();
		ListView_SetItem(hListView, &item);

		i++;
	}
}


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
				AppendMenu(hMenu, MF_STRING, CMD_SEQUENCE, "Sequence List");
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
			IB.element[0].Initialize = ((config.all_replace != 0) ? SetChecked : NULL);

			IB.element[1].itemtype = "list";
			IB.element[1].itemtext = "";
			IB.element[1].x = 30;
			IB.element[1].y = 25;
			IB.element[1].width = 260;
			IB.element[1].height = 60;
			IB.element[1].Initialize = PopulateKeyList;
			IB.element[1].initialze_arg = &(config.all_replace);

			IB.element[2].itemtype = "radio";
			IB.element[2].itemtext = "Use Caps Lock sequences";
			IB.element[2].x = 10;
			IB.element[2].y = 95;
			IB.element[2].width = 280;
			IB.element[2].height = 10;
			IB.element[2].Initialize = ((config.all_replace == 0) ? SetChecked : NULL);

			IB.element[3].itemtype = "button";
			IB.element[3].itemtext = "OK";
			IB.element[3].x = 30;
			IB.element[3].y = 120;
			IB.element[3].width = 100;
			IB.element[3].height = 15;
			IB.element[4].itemtype = "button";
			IB.element[4].itemtext = "Cancel";
			IB.element[4].x = 170;
			IB.element[4].y = 120;
			IB.element[4].width = 100;
			IB.element[4].height = 15;

			int r = InputBox(&IB);
			if ((r == 1) && (IB.button_result == 1)) {
				/* r==1 .. Inputbox closed by button */
				/* IB.result==1 .. Inputbox closed by first button ("OK") */
				if ((IB.element[0].value.n == 1) && (IB.element[1].value.n > 0)) {
					config.all_replace = (uint8_t)(IB.element[1].value.n);
				} else {
					config.all_replace = 0;
				}
			}
			return 0;
		}
		case CMD_SEQUENCE: {
			char title[64];
			sprintf(title, "%s Sequences", CAPS2useful);
			struct INPUTBOX IB;
			memset(&IB, 0, sizeof(IB));
			IB.x = 100;
			IB.y = 100;
			IB.width = 300;
			IB.height = 150;
			IB.title = title;
			IB.no_elements = 1;

			IB.element[0].itemtype = "list";
			IB.element[0].itemtext = "";
			IB.element[0].x = 10;
			IB.element[0].y = 10;
			IB.element[0].width = 290;
			IB.element[0].height = 140;
			IB.element[0].Initialize = PopulateHotkeyList;
			IB.element[0].initialze_arg = NULL;

			InputBox(&IB);

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

	if ((len < (MAX_WPATH - 7)) && (len > 4) && (0 == wcsicmp(path + len - 4, L".exe"))) {
		wcscpy(path + len - 4, L".keymap");
		FILE *f = _wfopen(path, L"rb");
		if (f != NULL) {
			BOOL isValid = TRUE;
			char fileKey[FILE_KEY_LEN] = {0};

			size_t bytesRead = fread(fileKey, 1, FILE_KEY_LEN, f);
			if (bytesRead != FILE_KEY_LEN) {
				isValid = FALSE;
			} else if (0 != memcmp(fileKey, FILE_KEY, FILE_KEY_LEN)) {
				isValid = FALSE;
			}

			if (!isValid) {
				wchar_t txt[256] = {0};
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
					MessageBoxA(NULL, "Critical error writing config file.", CAPS2useful, MB_ICONERROR);
					return FALSE;
				}
			}

			fseek(f, FILE_KEY_LEN, SEEK_SET);

			/* Read File line by line */
			char line[1024] = {0};
			while (fgets(line, sizeof(line), f) != NULL) {
				char *c = line;
				while (isspace(*c)) {
					c++;
				}
				if ((!*c) || (*c == '#')) {
					continue;
				}
				char *eol = strchr(c, '\r');
				if (eol) *eol = 0;
				eol = strchr(c, '\n');
				if (eol) *eol = 0;
				char *t = strchr(c, '\t');
				while (t) {
					*t = ' ';
					t = strchr(c, '\t');
				}
				char *ss = strstr(c, "  ");
				while (ss) {
					size_t l = strlen(ss);
					memmove(ss, ss + 1, l);
					ss = strstr(c, "  ");
				}

				/* Process buffer */
				if (0 == memcmp("ALL ", c, 4)) {
					int8_t hi = fromHex(c[4]);
					int8_t lo = fromHex(c[5]);
					if ((hi >= 0) && (lo >= 0)) {
						config.all_replace = ((uint8_t)hi << 4) + (uint8_t)lo;
					}

				} else if (0 == memcmp("S ", c, 2)) {
					trigger trig;
					int len;
					for (len = 0; len < MAX_KEYLEN; len++) {
						int8_t hi = fromHex(c[2 * len + 2]);
						int8_t lo = fromHex(c[2 * len + 3]);
						uint8_t hex = 0;
						if ((hi >= 0) && (lo >= 0)) {
							hex = ((uint8_t)hi << 4) + (uint8_t)lo;
							trig.push_back(hex);
						}
						if (hex == 0)
							break;
					}
					if (len > 0) {
						char *cmd = c + 2 * len + 2;
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
						if (isalpha(act.actType)) {
							config.hotkey[trig] = act;
						}
					}
				}
			}

			fclose(f);

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

	HANDLE hObj = CreateMutexA(&sa, FALSE, CAPS2useful);
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

	INITCOMMONCONTROLSEX ic;
	ic.dwSize = sizeof(ic);
	ic.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES;
	InitCommonControlsEx(&ic);

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
