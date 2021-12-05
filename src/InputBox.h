#pragma once
#include "Windows.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct INPUTBOXELEMENT {
	const char *itemtype;
	const char *itemtext;
	uint16_t x, y, width, height;
};


struct INPUTVALUE {
	enum { _void, _bool, _nr, _str } type;
	union {
		long long n;
		char s[1024];
	};
};

struct INPUTBOX {
	const char *title;
	uint16_t x, y, width, height;

	int result;

	uint16_t no_values;
	struct INPUTVALUE value[100];

	uint16_t no_elements;
	struct INPUTBOXELEMENT element[100];
};


int InputBox(struct INPUTBOX *B);

void PopulateKeyList(HWND hListBox);

#ifdef __cplusplus
}
#endif
