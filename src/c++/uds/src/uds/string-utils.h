/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright Red Hat
 */

#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/string.h>
#else
#include <stdarg.h>
#include <stdio.h> /* for vsnprintf */
#include <stdlib.h> /* for strtol */
#include <string.h>
#endif

#include "compiler.h"
#include "type-defs.h"

/**
 * Convert a boolean value to its corresponding "true" or "false" string.
 *
 * @param value  The boolean value to convert
 *
 * @return "true" if value is true, "false" otherwise.
 **/
static INLINE const char *uds_bool_to_string(bool value)
{
	return (value ? "true" : "false");
}

#if !defined(__KERNEL__) || defined(TEST_INTERNAL)
/**
 * Allocate a string built according to format (our version of asprintf).
 *
 * @param [in]  what    A description of what is being allocated, for error
 *                      logging; if NULL doesn't log anything.
 * @param [out] strp    The pointer in which to store the allocated string.
 * @param [in]  fmt     The sprintf format parameter.
 *
 * @return UDS_SUCCESS, or the appropriately translated asprintf error
 **/
int __must_check uds_alloc_sprintf(const char *what,
				   char **strp,
				   const char *fmt, ...)
	__printf(3, 4);

#endif /* (not __KERNEL) or TEST_INTERNAL */
/**
 * Write a printf-style string into a fixed-size buffer, returning
 * errors if it would not fit. (our version of snprintf)
 *
 * @param [in]  what     A description of what is being written, for error
 *                       logging; if NULL doesn't log anything.
 * @param [out] buf      The target buffer
 * @param [in]  buf_size The size of buf
 * @param [in]  error    Error code to return on overflow
 * @param [in]  fmt      The sprintf format parameter.
 *
 * @return <code>UDS_SUCCESS</code> or <code>error</code>
 **/
int __must_check uds_fixed_sprintf(const char *what,
				   char *buf,
				   size_t buf_size,
				   int error,
				   const char *fmt, ...)
	__printf(5, 6);

/**
 * Write printf-style string into an existing buffer, returning a specified
 * error code if it would not fit, and setting ``needed`` to the amount of
 * space that would be required.
 *
 * @param [in]  what     A description of what is being written, for logging.
 * @param [in]  buf      The buffer in which to write the string, or NULL to
 *                       merely determine the required space.
 * @param [in]  buf_size The size of buf.
 * @param [in]  error    The error code to return for exceeding the specified
 *                       space, UDS_SUCCESS if no logging required.
 * @param [in]  fmt      The sprintf format specification.
 * @param [in]  ap       The variable argument pointer (see <stdarg.h>).
 * @param [out] needed   If non-NULL, the actual amount of string space
 *                       required, which may be smaller or larger than
 *                       buf_size.
 *
 * @return UDS_SUCCESS if the string fits, the value of the error parameter if
 *         the string does not fit and a buffer was supplied, or
 *         UDS_UNEXPECTED_RESULT if vsnprintf fails in some other undocumented
 *         way.
 **/
int __must_check uds_wrap_vsnprintf(const char *what,
				    char *buf,
				    size_t buf_size,
				    int error,
				    const char *fmt,
				    va_list ap,
				    size_t *needed)
	__printf(5, 0);

/**
 * Helper to append a string to a buffer.
 *
 * @param buffer        the place at which to append the string
 * @param buf_end       pointer to the end of the buffer
 * @param fmt           a printf format string
 *
 * @return      the updated buffer position after the append
 *
 * if insufficient space is available, the contents are silently truncated
 **/
char *uds_append_to_buffer(char *buffer, char *buf_end, const char *fmt, ...)
	__printf(3, 4);

/**
 * Variable-arglist helper to append a string to a buffer.
 *
 * @param buffer   the place at which to append the string
 * @param buf_end  pointer to the end of the buffer
 * @param fmt      a printf format string
 * @param args     printf arguments
 *
 * @return the updated buffer position after the append
 *
 * if insufficient space is available, the contents are silently truncated
 **/
char *uds_v_append_to_buffer(char *buffer, char *buf_end, const char *fmt,
			     va_list args)
	__printf(3, 0);

#ifndef __KERNEL__
/**
 * Parse a string representing a decimal uint64_t.
 *
 * @param str           The string.
 * @param num           Where to put the number.
 *
 * @return UDS_SUCCESS or the error UDS_INVALID_ARGUMENT if the string
 *         is not in the correct format.
 **/
int __must_check uds_parse_uint64(const char *str, uint64_t *num);

/**
 * Attempt to convert a string to an integer (base 10)
 *
 * @param nptr  Pointer to string to convert
 * @param num   The resulting integer
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check uds_string_to_signed_int(const char *nptr, int *num);

/**
 * Attempt to convert a string to a long integer (base 10)
 *
 * @param nptr  Pointer to string to convert
 * @param num   The resulting long integer
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check uds_string_to_signed_long(const char *nptr, long *num);

/**
 * Attempt to convert a string to an unsigned integer (base 10).
 *
 * @param nptr  Pointer to string to convert
 * @param num   The resulting unsigned integer
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check
uds_string_to_unsigned_int(const char *nptr, unsigned int *num);

/**
 * Attempt to convert a string to an unsigned long integer (base 10).
 *
 * @param nptr  Pointer to string to convert
 * @param num   The resulting long unsigned integer
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check
uds_string_to_unsigned_long(const char *nptr, unsigned long *num);
#endif /* not  __KERNEL__ */

#endif /* STRING_UTILS_H */
