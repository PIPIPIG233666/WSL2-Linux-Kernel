/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_FORTIFY_STRING_H_
#define _LINUX_FORTIFY_STRING_H_

#define __FORTIFY_INLINE extern __always_inline __attribute__((gnu_inline))
#define __RENAME(x) __asm__(#x)

void fortify_panic(const char *name) __noreturn __cold;
void __read_overflow(void) __compiletime_error("detected read beyond size of object (1st parameter)");
void __read_overflow2(void) __compiletime_error("detected read beyond size of object (2nd parameter)");
void __read_overflow2_field(size_t avail, size_t wanted) __compiletime_warning("detected read beyond size of field (2nd parameter); maybe use struct_group()?");
void __write_overflow(void) __compiletime_error("detected write beyond size of object (1st parameter)");
void __write_overflow_field(size_t avail, size_t wanted) __compiletime_warning("detected write beyond size of field (1st parameter); maybe use struct_group()?");

#define __compiletime_strlen(p)					\
({								\
	unsigned char *__p = (unsigned char *)(p);		\
	size_t __ret = (size_t)-1;				\
	size_t __p_size = __builtin_object_size(p, 1);		\
	if (__p_size != (size_t)-1) {				\
		size_t __p_len = __p_size - 1;			\
		if (__builtin_constant_p(__p[__p_len]) &&	\
		    __p[__p_len] == '\0')			\
			__ret = __builtin_strlen(__p);		\
	}							\
	__ret;							\
})

#if defined(CONFIG_KASAN_GENERIC) || defined(CONFIG_KASAN_SW_TAGS)
extern void *__underlying_memchr(const void *p, int c, __kernel_size_t size) __RENAME(memchr);
extern int __underlying_memcmp(const void *p, const void *q, __kernel_size_t size) __RENAME(memcmp);
extern void *__underlying_memcpy(void *p, const void *q, __kernel_size_t size) __RENAME(memcpy);
extern void *__underlying_memmove(void *p, const void *q, __kernel_size_t size) __RENAME(memmove);
extern void *__underlying_memset(void *p, int c, __kernel_size_t size) __RENAME(memset);
extern char *__underlying_strcat(char *p, const char *q) __RENAME(strcat);
extern char *__underlying_strcpy(char *p, const char *q) __RENAME(strcpy);
extern __kernel_size_t __underlying_strlen(const char *p) __RENAME(strlen);
extern char *__underlying_strncat(char *p, const char *q, __kernel_size_t count) __RENAME(strncat);
extern char *__underlying_strncpy(char *p, const char *q, __kernel_size_t size) __RENAME(strncpy);
#else
#define __underlying_memchr	__builtin_memchr
#define __underlying_memcmp	__builtin_memcmp
#define __underlying_memcpy	__builtin_memcpy
#define __underlying_memmove	__builtin_memmove
#define __underlying_memset	__builtin_memset
#define __underlying_strcat	__builtin_strcat
#define __underlying_strcpy	__builtin_strcpy
#define __underlying_strlen	__builtin_strlen
#define __underlying_strncat	__builtin_strncat
#define __underlying_strncpy	__builtin_strncpy
#endif

#define strncpy(p, q, s) __fortify_strncpy(p, q, s, __builtin_object_size(p, 1))
__FORTIFY_INLINE char *__fortify_strncpy(char *p, const char *q,
					 __kernel_size_t size, size_t p_size)
{
	if (__builtin_constant_p(size) && p_size < size)
		__write_overflow();
	if (p_size < size)
		fortify_panic(__func__);
	return __underlying_strncpy(p, q, size);
}

__FORTIFY_INLINE char *strcat(char *p, const char *q)
{
	size_t p_size = __builtin_object_size(p, 1);

	if (p_size == (size_t)-1)
		return __underlying_strcat(p, q);
	if (strlcat(p, q, p_size) >= p_size)
		fortify_panic(__func__);
	return p;
}

extern __kernel_size_t __real_strnlen(const char *, __kernel_size_t) __RENAME(strnlen);
#define strnlen(p, s) __fortify_strnlen(p, s, __builtin_object_size(p, 1))
__FORTIFY_INLINE __kernel_size_t __fortify_strnlen(const char *p, size_t maxlen,
						   size_t p_size)
{
	size_t p_len = __compiletime_strlen(p);
	size_t ret;

	/* We can take compile-time actions when maxlen is const. */
	if (__builtin_constant_p(maxlen) && p_len != (size_t)-1) {
		/* If p is const, we can use its compile-time-known len. */
		if (maxlen >= p_size)
			return p_len;
	}

	/* Do not check characters beyond the end of p. */
	ret = __real_strnlen(p, maxlen < p_size ? maxlen : p_size);
	if (p_size <= ret && maxlen != ret)
		fortify_panic(__func__);
	return ret;
}

/* defined after fortified strnlen to reuse it. */
#define strlen(p) __fortify_strlen(p, __builtin_object_size(p, 1))
__FORTIFY_INLINE __kernel_size_t __fortify_strlen(const char *p, const size_t p_size)
{
	__kernel_size_t ret;

	/* Give up if we don't know how large p is. */
	if (p_size == (size_t)-1)
		return __underlying_strlen(p);
	ret = strnlen(p, p_size);
	if (p_size <= ret)
		fortify_panic(__func__);
	return ret;
}

/* defined after fortified strlen to reuse it */
extern size_t __real_strlcpy(char *, const char *, size_t) __RENAME(strlcpy);
#define strlcpy(p, q, s) __fortify_strlcpy(p, q, s,			\
					   __builtin_object_size(p, 1),	\
					   __builtin_object_size(q, 1))
__FORTIFY_INLINE size_t __fortify_strlcpy(char *p, const char *q,
					  size_t size,
					  const size_t p_size,
					  const size_t q_size)
{
	size_t q_len;	/* Full count of source string length. */
	size_t len;	/* Count of characters going into destination. */

	if (p_size == (size_t)-1 && q_size == (size_t)-1)
		return __real_strlcpy(p, q, size);
	q_len = strlen(q);
	len = (q_len >= size) ? size - 1 : q_len;
	if (__builtin_constant_p(size) && __builtin_constant_p(q_len) && size) {
		/* Write size is always larger than destination. */
		if (len >= p_size)
			__write_overflow();
	}
	if (size) {
		if (len >= p_size)
			fortify_panic(__func__);
		__underlying_memcpy(p, q, len);
		p[len] = '\0';
	}
	return q_len;
}

/* defined after fortified strnlen to reuse it */
extern ssize_t __real_strscpy(char *, const char *, size_t) __RENAME(strscpy);
#define strscpy(p, q, s) __fortify_strscpy(p, q, s,			\
					   __builtin_object_size(p, 1),	\
					   __builtin_object_size(q, 1))
__FORTIFY_INLINE ssize_t __fortify_strscpy(char *p, const char *q,
					   size_t size,
					   const size_t p_size,
					   const size_t q_size)
{
	size_t len;

	/* If we cannot get size of p and q default to call strscpy. */
	if (p_size == (size_t) -1 && q_size == (size_t) -1)
		return __real_strscpy(p, q, size);

	/*
	 * If size can be known at compile time and is greater than
	 * p_size, generate a compile time write overflow error.
	 */
	if (__builtin_constant_p(size) && size > p_size)
		__write_overflow();

	/*
	 * This call protects from read overflow, because len will default to q
	 * length if it smaller than size.
	 */
	len = strnlen(q, size);
	/*
	 * If len equals size, we will copy only size bytes which leads to
	 * -E2BIG being returned.
	 * Otherwise we will copy len + 1 because of the final '\O'.
	 */
	len = len == size ? size : len + 1;

	/*
	 * Generate a runtime write overflow error if len is greater than
	 * p_size.
	 */
	if (len > p_size)
		fortify_panic(__func__);

	/*
	 * We can now safely call vanilla strscpy because we are protected from:
	 * 1. Read overflow thanks to call to strnlen().
	 * 2. Write overflow thanks to above ifs.
	 */
	return __real_strscpy(p, q, len);
}

/* defined after fortified strlen and strnlen to reuse them */
#define strncat(p, q, count)	__fortify_strncat(p, q, count, \
						  __builtin_object_size(p, 1), \
						  __builtin_object_size(q, 1))
__FORTIFY_INLINE char *__fortify_strncat(char *p, const char *q, size_t count,
					 size_t p_size, size_t q_size)
{
	size_t p_len, copy_len;

	if (p_size == (size_t)-1 && q_size == (size_t)-1)
		return __underlying_strncat(p, q, count);
	p_len = strlen(p);
	copy_len = strnlen(q, count);
	if (p_size < p_len + copy_len + 1)
		fortify_panic(__func__);
	__underlying_memcpy(p + p_len, q, copy_len);
	p[p_len + copy_len] = '\0';
	return p;
}

__FORTIFY_INLINE void fortify_memset_chk(__kernel_size_t size,
					 const size_t p_size,
					 const size_t p_size_field)
{
	if (__builtin_constant_p(size)) {
		/*
		 * Length argument is a constant expression, so we
		 * can perform compile-time bounds checking where
		 * buffer sizes are known.
		 */

		/* Error when size is larger than enclosing struct. */
		if (p_size > p_size_field && p_size < size)
			__write_overflow();

		/* Warn when write size is larger than dest field. */
		if (p_size_field < size)
			__write_overflow_field(p_size_field, size);
	}
	/*
	 * At this point, length argument may not be a constant expression,
	 * so run-time bounds checking can be done where buffer sizes are
	 * known. (This is not an "else" because the above checks may only
	 * be compile-time warnings, and we want to still warn for run-time
	 * overflows.)
	 */

	/*
	 * Always stop accesses beyond the struct that contains the
	 * field, when the buffer's remaining size is known.
	 * (The -1 test is to optimize away checks where the buffer
	 * lengths are unknown.)
	 */
	if (p_size != (size_t)(-1) && p_size < size)
		fortify_panic("memset");
}

#define __fortify_memset_chk(p, c, size, p_size, p_size_field) ({	\
	size_t __fortify_size = (size_t)(size);				\
	fortify_memset_chk(__fortify_size, p_size, p_size_field),	\
	__underlying_memset(p, c, __fortify_size);			\
})

/*
 * __builtin_object_size() must be captured here to avoid evaluating argument
 * side-effects further into the macro layers.
 */
#define memset(p, c, s) __fortify_memset_chk(p, c, s,			\
		__builtin_object_size(p, 0), __builtin_object_size(p, 1))

/*
 * To make sure the compiler can enforce protection against buffer overflows,
 * memcpy(), memmove(), and memset() must not be used beyond individual
 * struct members. If you need to copy across multiple members, please use
 * struct_group() to create a named mirror of an anonymous struct union.
 * (e.g. see struct sk_buff.)
 *
 * Mitigation coverage
 *					Bounds checking at:
 *					+-------+-------+-------+-------+
 *					| Compile time  | Run time      |
 * memcpy() argument sizes:		| write | read  | write | read  |
 *					+-------+-------+-------+-------+
 * memcpy(known,   known,   constant)	|   y   |   y   |  n/a  |  n/a  |
 * memcpy(unknown, known,   constant)	|   n   |   y   |   V   |  n/a  |
 * memcpy(known,   unknown, constant)	|   y   |   n   |  n/a  |   V   |
 * memcpy(unknown, unknown, constant)	|   n   |   n   |   V   |   V   |
 * memcpy(known,   known,   dynamic)	|   n   |   n   |   b   |   B   |
 * memcpy(unknown, known,   dynamic)	|   n   |   n   |   V   |   B   |
 * memcpy(known,   unknown, dynamic)	|   n   |   n   |   b   |   V   |
 * memcpy(unknown, unknown, dynamic)	|   n   |   n   |   V   |   V   |
 *					+-------+-------+-------+-------+
 *
 * y = deterministic compile-time bounds checking
 * n = cannot do deterministic compile-time bounds checking
 * n/a = no run-time bounds checking needed since compile-time deterministic
 * b = perform run-time bounds checking
 * B = can perform run-time bounds checking, but current unenforced
 * V = vulnerable to run-time overflow
 *
 */
__FORTIFY_INLINE void fortify_memcpy_chk(__kernel_size_t size,
					 const size_t p_size,
					 const size_t q_size,
					 const size_t p_size_field,
					 const size_t q_size_field,
					 const char *func)
{
	if (__builtin_constant_p(size)) {
		/*
		 * Length argument is a constant expression, so we
		 * can perform compile-time bounds checking where
		 * buffer sizes are known.
		 */

		/* Error when size is larger than enclosing struct. */
		if (p_size > p_size_field && p_size < size)
			__write_overflow();
		if (q_size > q_size_field && q_size < size)
			__read_overflow2();

		/* Warn when write size argument larger than dest field. */
		if (p_size_field < size)
			__write_overflow_field(p_size_field, size);
		/*
		 * Warn for source field over-read when building with W=1
		 * or when an over-write happened, so both can be fixed at
		 * the same time.
		 */
		if ((IS_ENABLED(KBUILD_EXTRA_WARN1) || p_size_field < size) &&
		    q_size_field < size)
			__read_overflow2_field(q_size_field, size);
	}
	/*
	 * At this point, length argument may not be a constant expression,
	 * so run-time bounds checking can be done where buffer sizes are
	 * known. (This is not an "else" because the above checks may only
	 * be compile-time warnings, and we want to still warn for run-time
	 * overflows.)
	 */

	/*
	 * Always stop accesses beyond the struct that contains the
	 * field, when the buffer's remaining size is known.
	 * (The -1 test is to optimize away checks where the buffer
	 * lengths are unknown.)
	 */
	if ((p_size != (size_t)(-1) && p_size < size) ||
	    (q_size != (size_t)(-1) && q_size < size))
		fortify_panic(func);
}

#define __fortify_memcpy_chk(p, q, size, p_size, q_size,		\
			     p_size_field, q_size_field, op) ({		\
	size_t __fortify_size = (size_t)(size);				\
	fortify_memcpy_chk(__fortify_size, p_size, q_size,		\
			   p_size_field, q_size_field, #op);		\
	__underlying_##op(p, q, __fortify_size);			\
})

/*
 * __builtin_object_size() must be captured here to avoid evaluating argument
 * side-effects further into the macro layers.
 */
#define memcpy(p, q, s)  __fortify_memcpy_chk(p, q, s,			\
		__builtin_object_size(p, 0), __builtin_object_size(q, 0), \
		__builtin_object_size(p, 1), __builtin_object_size(q, 1), \
		memcpy)
#define memmove(p, q, s)  __fortify_memcpy_chk(p, q, s,			\
		__builtin_object_size(p, 0), __builtin_object_size(q, 0), \
		__builtin_object_size(p, 1), __builtin_object_size(q, 1), \
		memmove)

extern void *__real_memscan(void *, int, __kernel_size_t) __RENAME(memscan);
#define memscan(p, c, s) __fortify_memscan(p, c, s)
__FORTIFY_INLINE void *__fortify_memscan(void *p, int c, __kernel_size_t size)
{
	size_t p_size = __builtin_object_size(p, 0);

	if (__builtin_constant_p(size) && p_size < size)
		__read_overflow();
	if (p_size < size)
		fortify_panic(__func__);
	return __real_memscan(p, c, size);
}

#define memcmp(p, q, s) __fortify_memcmp(p, q, s)
__FORTIFY_INLINE int __fortify_memcmp(const void *p, const void *q, __kernel_size_t size)
{
	size_t p_size = __builtin_object_size(p, 0);
	size_t q_size = __builtin_object_size(q, 0);

	if (__builtin_constant_p(size)) {
		if (p_size < size)
			__read_overflow();
		if (q_size < size)
			__read_overflow2();
	}
	if (p_size < size || q_size < size)
		fortify_panic(__func__);
	return __underlying_memcmp(p, q, size);
}

#define memchr(p, c, s) __fortify_memchr(p, c, s)
__FORTIFY_INLINE void *__fortify_memchr(const void *p, int c, __kernel_size_t size)
{
	size_t p_size = __builtin_object_size(p, 0);

	if (__builtin_constant_p(size) && p_size < size)
		__read_overflow();
	if (p_size < size)
		fortify_panic(__func__);
	return __underlying_memchr(p, c, size);
}

void *__real_memchr_inv(const void *s, int c, size_t n) __RENAME(memchr_inv);
#define memchr_inv(p, c, s) __fortify_memchr_inv(p, c, s)
__FORTIFY_INLINE void *__fortify_memchr_inv(const void *p, int c, size_t size)
{
	size_t p_size = __builtin_object_size(p, 0);

	if (__builtin_constant_p(size) && p_size < size)
		__read_overflow();
	if (p_size < size)
		fortify_panic(__func__);
	return __real_memchr_inv(p, c, size);
}

extern void *__real_kmemdup(const void *src, size_t len, gfp_t gfp) __RENAME(kmemdup);
__FORTIFY_INLINE void *kmemdup(const void *p, size_t size, gfp_t gfp)
{
	size_t p_size = __builtin_object_size(p, 0);

	if (__builtin_constant_p(size) && p_size < size)
		__read_overflow();
	if (p_size < size)
		fortify_panic(__func__);
	return __real_kmemdup(p, size, gfp);
}

/* Defined after fortified strlen to reuse it. */
#define strcpy(p, q) __fortify_strcpy(p, q,				\
				      __builtin_object_size(p, 1),	\
				      __builtin_object_size(q, 1))
__FORTIFY_INLINE char *__fortify_strcpy(char *p, const char *q,
					const size_t p_size,
					const size_t q_size)
{
	size_t size;

	/* If neither buffer size is known, immediately give up. */
	if (p_size == (size_t)-1 && q_size == (size_t)-1)
		return __underlying_strcpy(p, q);
	size = strlen(q) + 1;
	/* Compile-time check for const size overflow. */
	if (__builtin_constant_p(size) && p_size < size)
		__write_overflow();
	/* Run-time check for dynamic size overflow. */
	if (p_size < size)
		fortify_panic(__func__);
	__underlying_memcpy(p, q, size);
	return p;
}

/* Don't use these outside the FORITFY_SOURCE implementation */
#undef __underlying_memchr
#undef __underlying_memcmp
#undef __underlying_strcat
#undef __underlying_strcpy
#undef __underlying_strlen
#undef __underlying_strncat
#undef __underlying_strncpy

#endif /* _LINUX_FORTIFY_STRING_H_ */
