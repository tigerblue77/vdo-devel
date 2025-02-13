/* SPDX-License-Identifier: LGPL-2.1+ */
/*
 * MurmurHash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
 */

#ifndef _MURMURHASH3_H_
#define _MURMURHASH3_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#include "compiler.h"
#endif

void murmurhash3_128(const void *key, int len, uint32_t seed, void *out);

#endif /* _MURMURHASH3_H_ */
