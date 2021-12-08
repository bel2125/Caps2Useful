#pragma once
#include "Windows.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct INPUTVALUE {
	enum { _void, _bool, _nr, _str } type;
	union {
		long long n;
		char s[1024];
	};
};

struct INPUTBOXELEMENT {
	const char *itemtype;
	const char *itemtext;
	uint16_t x, y, width, height;

	struct INPUTVALUE value;
	void (*Initialize)(HWND, void *);
	void *initialze_arg;
};


struct INPUTBOX {
	const char *title;
	uint16_t x, y, width, height;

	int button_result;

	uint16_t no_elements;
	struct INPUTBOXELEMENT element[100]; /* must be last element */
};


int InputBox(struct INPUTBOX *B);

#ifdef __cplusplus
}
#endif
