/*
 *  Six libc symbols that jxrlib references and solver_minimal_1 does not
 *  export. A side module whose imports are not all resolved never instantiates
 *  at all - the filter simply never registers - so each one has to be defined
 *  here (the approach libape/dec_ape.cpp already takes for its wide-char
 *  functions).
 *
 *  Kept in a translation unit of its own, free of GPAC headers, for the same
 *  reason as jxr_decode.c: jxrlib and GPAC both define Bool, incompatibly.
 *
 *  Two groups:
 *
 *  - puts, wcslen and rand are implemented faithfully. puts is layered on
 *    fputs/stderr, both of which the solver does export.
 *  - feof and tmpnam only ever run on jxrlib's stdio stream backend, which
 *    this filter never creates: it decodes from memory through
 *    PKFactory::CreateStreamFromMemory. They are pulled in because the encoder
 *    objects (encode.o, strenc.o, segenc.o) share strcodec with the decoder,
 *    not because the decode path calls them. They are defined so the module
 *    links, and behave conservatively if that ever stops being true.
 */

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

int puts(const char *s)
{
	if (s)
		fputs(s, stderr);
	fputs("\n", stderr);
	return 0;
}

size_t wcslen(const wchar_t *s)
{
	const wchar_t *p = s;
	while (*p)
		p++;
	return (size_t)(p - s);
}

/* jxrlib only uses rand() to break ties when quantising, so any decent
 * sequence will do; this is the LCG from the C standard's example. */
static unsigned long jxr_rand_state = 1;

int rand(void)
{
	jxr_rand_state = jxr_rand_state * 1103515245 + 12345;
	return (int)((jxr_rand_state / 65536) % 32768);
}

void srand(unsigned int seed)
{
	jxr_rand_state = seed;
}

/* Unreachable here (see the header comment). "not at end of file" is the
 * answer that keeps a caller reading rather than silently truncating. */
int feof(FILE *stream)
{
	(void)stream;
	return 0;
}

/* Unreachable here. NULL is tmpnam's documented failure return, which callers
 * are required to handle. */
char *tmpnam(char *s)
{
	(void)s;
	return NULL;
}

void __assert_fail(const char *expr, const char *file, unsigned int line, const char *func)
{
	fprintf(stderr, "[JXRDec] assertion failed: %s at %s:%u in %s\n",
	        expr ? expr : "?", file ? file : "?", line, func ? func : "?");
	abort();
}
