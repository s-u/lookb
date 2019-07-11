/*-
 * Copyright (c) 2019 Simon Urbanek
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <Rinternals.h>

static const unsigned char *key;
static unsigned char *head;
static int key_len;

/* 0: match, >0: content is greater than key, <0: content is smaller than key */
static int compare(const unsigned char *front, const unsigned char *back) {
    if (back - front >= key_len) /* if the content is long enough, direct comparison */
	return memcmp(front, key, key_len);
    return (memcmp(front, key, back - front) <= 0) ? -1 : 1; /* it cannot be a match, incomplete match is still smaller */
}

#ifdef DEBUG
#define DEBUGME(X, POS) { char tmp[128], *nl; if (POS) memcpy(tmp, POS, 40); else strcpy(tmp, "<NULL>"); tmp[40]=0; if ((nl = strchr(tmp, '\n'))) *nl = 0; fprintf(stderr, "%s:[%ld,%ld] %s...\n", X, POS - head, back - head, tmp); }
#else
#define DEBUGME(X, POS)
#endif


static const unsigned char *
binary_search(const unsigned char *front, const unsigned char *back)
{
    /* if the remaining window is small, don't bother and rely on linear search */
    while (back - front > 8192) {
	const unsigned char *t,  *p = front + (back - front) / 2;
	int m;
	DEBUGME("binary_search", front);
	if (p >= back)
	    return 0;

	/* find the first beginning of the line past half */
	if (!(t = memchr(p, '\n', back - p))) {
	    /* no newline past the midpoint? try from the front */
	    if (!(t = memchr(front, '\n', back - p))) /* no newline at all */
		return front;
	}
	p = t + 1;
	DEBUGME("binary_search_pivot", p);
	m = compare(p, back);
	if (m < 0) /* if the pivot is strictly smaller, look in the second half */
	    front = p;
	else if (m == 0)  /* we just treat it as inclusive back; we could do something smarter such as backward search ... */
	    back = p + 1;
	else /* strictly greater, look in the front */
	    back = p;
    }
    return front;
}

static const unsigned char *
linear_search(const unsigned char *front, const unsigned char *back)
{
    int cmp;
    /* skip until match */
    while (front < back && (cmp = compare(front, back)) < 0) {
	front = memchr(front, '\n', back - front);
	if (!front)
	    return 0;
	front++;
    }
    return cmp ? 0 : front;
}

SEXP raw_look(SEXP filename, SEXP sKey, SEXP nSkip, SEXP sMaxLines) {
    SEXP output;
    struct stat sb;
    int fd, j;
    long skip = (long) asReal(nSkip);
    long maxLines = (long) asReal(sMaxLines);
    long nrows = 0;
    const unsigned char *back, *front, *res;
    const char *file;

    if (skip < 0)
	Rf_error("Invalid number of lines to skip, must be 0 or positive number");

    if (TYPEOF(filename) != STRSXP || LENGTH(filename) != 1)
	Rf_error("Invalid filename, must be a string.");
    file = CHAR(STRING_ELT(filename, 0));

    if (TYPEOF(sKey) != STRSXP || LENGTH(sKey) != 1)
	Rf_error("Invalid key, must be a string.");
    key = (const unsigned char*) CHAR(STRING_ELT(sKey, 0));
    key_len = strlen((const char*) key);

    if ((fd = open(file, O_RDONLY, 0)) < 0 || fstat(fd, &sb))
	Rf_error("Unable to open '%s' for reading (%s).", file, strerror(errno));

    if (sb.st_size == 0) {
	close(fd);
	return allocVector(RAWSXP, 0);
    }

    if ((front = head = mmap(NULL, (size_t)sb.st_size, PROT_READ, MAP_SHARED, fd, (off_t)0)) == MAP_FAILED)
	Rf_error("Failed to mmap %s: %s", file, strerror(errno));

    back = front + sb.st_size;

    /* Skip header rows if requested */
    j = 0;
    while (j < skip) {
	/* if that takes us past the end, return non-match */
	if (!(front = ((const unsigned char*)memchr(front, '\n', back - front)) + 1)) {
	    munmap(head, back - head);
	    return allocVector(RAWSXP, 0);
	}
    }

    front = binary_search(front, back);
    DEBUGME("binary_search_result", front);
    if (front) {
	front = linear_search(front, back);
	DEBUGME("linear_search", front);
    }

    close(fd);

    if (front == NULL) { /* no match */
	munmap(head, back - head);
	return allocVector(RAWSXP, 0);
    }

    /* we got the first match, now collect all rows that match */
    res = front;
    while (front < back && compare(front, back) == 0) {
	nrows++;
	front = memchr(front, '\n', back - front);
        if (!front) {
	    front = back;
	    break;
	}
	front++;
	if (maxLines > 0 && nrows >= maxLines)
	    break;
    }
    output = allocVector(RAWSXP, front - res);
    memcpy(RAW(output), res, front - res);

    munmap(head, back - head);

    return output;
}
