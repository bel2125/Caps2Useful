#ifndef KEYLIST_H
#define KEYLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct tVK_names {
	uint8_t id;
	const char *name;
	const char *descr;
};

extern struct tVK_names vkey_names[];
const char *GetKeyName(uint8_t a);

#ifdef __cplusplus
}
#endif

#endif
