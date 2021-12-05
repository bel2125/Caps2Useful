#include "InputBox.h"
#include <Windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define stricmp _stricmp

/* LPARAM pointer passed to WM_INITDIALOG */
struct dlg_proc_param {
	HWND hWnd;
	struct INPUTBOX *B;
	int listBoxId;
	int ret;
};

static INT_PTR CALLBACK
DialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {

	case WM_CLOSE: {
		struct dlg_proc_param *pdlg_proc_param = (struct dlg_proc_param *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
		if ((pdlg_proc_param->hWnd == hDlg) && (pdlg_proc_param->B != NULL)) {
			// condition should always be true
			pdlg_proc_param->B->result = 0;
			pdlg_proc_param->ret = 1;
		}
		DestroyWindow(hDlg);
	} break;

	case WM_COMMAND: {
		WORD wmId = LOWORD(wParam);
		WORD wmEvent = HIWORD(wParam);
		HWND dlg = GetDlgItem(hDlg, wmId);

		if (wmEvent == 0) {
			if (wmId >= 0x1000) {
				int result = wmId - 0x1000;
				struct dlg_proc_param *pdlg_proc_param = (struct dlg_proc_param *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
				if ((pdlg_proc_param->hWnd == hDlg) && (pdlg_proc_param->B != NULL)) {
					// condition should always be true
					pdlg_proc_param->B->result = result;
					pdlg_proc_param->ret = 2;
					int tabIndex = 0;

					for (int i = 1; i < 0x1000; i++) {
						HWND hItem = GetDlgItem(hDlg, i);
						if (!hItem)
							break;
						char className[32] = {0};
						GetClassName(hItem, className, sizeof(className));

						if (!stricmp(className, "Button")) {

							UINT r = IsDlgButtonChecked(hDlg, i);
							pdlg_proc_param->B->value[pdlg_proc_param->B->no_values].type = _bool;
							if (r == BST_CHECKED) {
								pdlg_proc_param->B->value[pdlg_proc_param->B->no_values++].n = 1;
							} else if (r == BST_UNCHECKED) {
								pdlg_proc_param->B->value[pdlg_proc_param->B->no_values++].n = 0;
							} else {
								pdlg_proc_param->B->value[pdlg_proc_param->B->no_values++].n = -1;
							}

						} else if (!stricmp(className, "Edit")) {

							DWORD dwStyle = (DWORD)GetWindowLong(hItem, GWL_STYLE);
							int len = GetWindowTextLengthW(hItem) + 1;
							LPWSTR utf16 = (LPWSTR)malloc(len * sizeof(WCHAR));
							if (!utf16)
								continue;

							GetWindowTextW(hItem, utf16, len);

							char *utf8 = (char *)malloc(len * 3);
							if (!utf8) {
								free(utf16);
								continue;
							}

							WideCharToMultiByte(CP_UTF8, 0, utf16, -1, utf8, len * 3, NULL, NULL);

							if ((dwStyle & ES_NUMBER) == ES_NUMBER) {
								long long nr = strtoll(utf8, NULL, 10);
								pdlg_proc_param->B->value[pdlg_proc_param->B->no_values].type = _nr;
								pdlg_proc_param->B->value[pdlg_proc_param->B->no_values++].n = nr;
							} else {
								pdlg_proc_param->B->value[pdlg_proc_param->B->no_values].type = _str;
								strcpy(pdlg_proc_param->B->value[pdlg_proc_param->B->no_values].s, utf8); /* XXX */
								pdlg_proc_param->B->no_values++;
							}

							free(utf8);
							free(utf16);
						} else if (!stricmp(className, "ListBox")) {
							int pos = SendMessage(hItem, LB_GETCURSEL, 0, 0);
							int nr = (int)SendMessage(hItem, LB_GETITEMDATA, pos, 0);
							pdlg_proc_param->B->value[pdlg_proc_param->B->no_values].type = _nr;
							pdlg_proc_param->B->value[pdlg_proc_param->B->no_values++].n = nr;
						}
					}
					// all parameters stored to Lua state
				}
				EndDialog(hDlg, result);
			}
		}
	} break;

	case WM_INITDIALOG: {
		struct dlg_proc_param *pdlg_proc_param = (struct dlg_proc_param *)lParam;
		pdlg_proc_param->hWnd = hDlg;
		pdlg_proc_param->ret = 0;
		SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)pdlg_proc_param);

		if (pdlg_proc_param->listBoxId) {
			HWND hListBox = GetDlgItem(hDlg, pdlg_proc_param->listBoxId);
			PopulateKeyList(hListBox);
		}

	} break;

	default:
		break;
	}

	return FALSE;
}

LPWORD
lpwAlign(LPWORD lpIn)
{
	ULONG align = 4;
	return (LPWORD)(((((size_t)lpIn) + 1) / align) * align);
}

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

int
InputBox(struct INPUTBOX *B)
{
	struct dlg_proc_param dlg_prm;
	memset(&dlg_prm, 0, sizeof(dlg_prm));
	dlg_prm.B = B;
	SHORT button_count = 0;
	SHORT input_count = 0;

	HINSTANCE hinst = NULL;
	HWND hwndOwner = NULL;

	HGLOBAL hgbl;
	LPDLGTEMPLATE lpdt;
	LPDLGITEMTEMPLATE lpdit;
	LPWORD lpw;
	LPWSTR lpwsz;
	LRESULT ret;
	int nchar;

	hgbl = GlobalAlloc(GMEM_ZEROINIT, 1024 * 16);
	if (!hgbl) {
		return -1;
	}
	lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);

	//-----------------------
	// Define a dialog box, including title.
	//-----------------------
	lpdt->style = WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION;

	/* get title */
	const char *title = B->title;

	/* get coordinates */
	lpdt->x = B->x;
	lpdt->y = B->y;
	lpdt->cx = B->width;
	lpdt->cy = B->height;

	lpw = (LPWORD)(lpdt + 1);
	*lpw++ = 0; // No menu
	*lpw++ = 0; // Predefined dialog box class (by default)

	lpwsz = (LPWSTR)lpw;
	nchar = 1 + MultiByteToWideChar(CP_UTF8, 0, title, -1, lpwsz, 100);
	lpw += nchar;
	lpdt->cdit = 0; // Number of items in dialog. Add them in a loop.

	//-----------------------
	// Dialog items
	//-----------------------
	for (int i = 0; i < (int)B->no_elements; i++) {

		// Add one dialog item
		lpw = lpwAlign(lpw); // Align DLGITEMTEMPLATE on DWORD boundary
		lpdit = (LPDLGITEMTEMPLATE)lpw;

		/* get coordinates */
		lpdit->x = B->element[i].x;
		lpdit->y = B->element[i].y;
		lpdit->cx = B->element[i].width;
		lpdit->cy = B->element[i].height;

		/* adjust dialog size */
		lpdt->cx = MAX(lpdt->cx, lpdit->x + lpdit->cx + 10);
		lpdt->cy = MAX(lpdt->cy, lpdit->y + lpdit->cy + 10);

		/* type/class */
		lpdt->cdit++;
		lpdit->id = 0; // Item identifier
		lpdit->style = WS_CHILD | WS_VISIBLE;
		lpdit->dwExtendedStyle = 0;

		// see
		// https://docs.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-dlgitemtemplate
		WORD dlg_class = 0x0082;
		if (!stricmp(B->element[i].itemtype, "button")) {
			button_count++;
			if (button_count == 1) {
				lpdit->style |= BS_DEFPUSHBUTTON;
			} else {
				lpdit->style |= BS_PUSHBUTTON;
			}
			dlg_class = 0x0080;
			lpdit->id = 0x1000 + button_count;
		}
		if (!stricmp(B->element[i].itemtype, "check") || !stricmp(B->element[i].itemtype, "boolean")) {
			input_count++;
			dlg_class = 0x0080;
			lpdit->style |= BS_AUTOCHECKBOX;
			lpdit->id = input_count;
		}
		if (!stricmp(B->element[i].itemtype, "radio")) {
			input_count++;
			dlg_class = 0x0080;
			lpdit->style |= BS_AUTORADIOBUTTON;
			lpdit->id = input_count;
		}
		if (!stricmp(B->element[i].itemtype, "3state")) {
			input_count++;
			dlg_class = 0x0080;
			lpdit->style |= BS_AUTO3STATE;
			lpdit->id = input_count;
		}
		if (!stricmp(B->element[i].itemtype, "edit") || !stricmp(B->element[i].itemtype, "string")) {
			input_count++;
			dlg_class = 0x0081;
			lpdit->style |= WS_BORDER | ES_AUTOHSCROLL;
			lpdit->id = input_count;
		}
		if (!stricmp(B->element[i].itemtype, "text") || !stricmp(B->element[i].itemtype, "multiline")) {
			input_count++;
			dlg_class = 0x0081;
			lpdit->style |= WS_BORDER | ES_AUTOHSCROLL | ES_MULTILINE;
			lpdit->style |= ES_WANTRETURN | WS_VSCROLL | ES_AUTOVSCROLL;
			lpdit->id = input_count;
		}
		if (!stricmp(B->element[i].itemtype, "number")) {
			input_count++;
			dlg_class = 0x0081;
			lpdit->style |= WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER;
			lpdit->id = input_count;
		}
		if (!stricmp(B->element[i].itemtype, "static") || !stricmp(B->element[i].itemtype, "label")) {
			dlg_class = 0x0082;
		}
		if (!stricmp(B->element[i].itemtype, "list")) {
			input_count++;
			dlg_class = 0x0083;
			lpdit->style |= WS_BORDER | WS_VSCROLL | ES_AUTOVSCROLL;
			lpdit->id = input_count;
			dlg_prm.listBoxId = input_count;
		}
#if 0
    if (!stricmp(B->element[i].itemtype, "scroll")) {
      dlg_class = 0x0084;
    }
	if (!stricmp(B->element[i].itemtype, "combo")) {
		dlg_class = 0x0085;
	}
#endif

		lpw = (LPWORD)(lpdit + 1);
		*lpw++ = 0xFFFF;
		*lpw++ = dlg_class;

		lpwsz = (LPWSTR)lpw;
		nchar = 1 + MultiByteToWideChar(CP_UTF8, 0, B->element[i].itemtext, -1, lpwsz, 100);
		lpw += nchar;
		*lpw++ = 0; // No creation data
	}

	// Set heigth, if not defined
	if (lpdt->cy < 0) {
		lpdt->cy = 20 + 20 * lpdt->cdit;
	}

	GlobalUnlock(hgbl);
	ret = DialogBoxIndirectParamA(hinst, (LPDLGTEMPLATE)hgbl, hwndOwner, (DLGPROC)DialogProc, (LPARAM)&dlg_prm);
	GlobalFree(hgbl);

	return dlg_prm.ret;
}
