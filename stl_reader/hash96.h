/*
The MIT License (MIT)

Copyright (c) 2016 Aki Nyrhinen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


static uint32_t
nextpow2(uint32_t val)
{
	val--; // we want to return 2 for 2, 4 for 4 etc.
	val |= val >> 1;
	val |= val >> 2;
	val |= val >> 4;
	val |= val >> 8;
	val |= val >> 16;
	return ++val;
}

/*
 *	These hash functions have been adapted from Bob Jenkins's
 *	excellent public domain work in http://burtleburtle.net/bob/c/lookup3.c
 *
 *	There are faster hash functions based on special instructions
 *	or if a fast multiplier is assumed, but these work fast enough
 *	in practice and produce consistent performance on lower end
 *	processors too.
 */
#define rot32(x,k) (((x)<<(k)) | ((x)>>(32-(k))))
static uint32_t
final96(uint32_t a, uint32_t b, uint32_t c)
{
	c ^= b; c -= rot32(b,14);
	a ^= c; a -= rot32(c,11);
	b ^= a; b -= rot32(a,25);
	c ^= b; c -= rot32(b,16);
	a ^= c; a -= rot32(c,4);
	b ^= a; b -= rot32(a,14);
	c ^= b; c -= rot32(b,24);
	return c;
}

static int
cmp96(uint32_t *a, uint32_t *b)
{
	return (a[0]-b[0]) | (a[1]-b[1]) | (a[2]-b[2]);
}

static void
copy96(uint32_t *dst, uint32_t *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}
#undef rot32
