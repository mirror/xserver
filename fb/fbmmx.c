/*
 * Copyright © 2004, 2005 Red Hat, Inc.
 * Copyright © 2004 Nicholas Miell
 * Copyright © 2005 Trolltech AS
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Author:  Søren Sandmann (sandmann@redhat.com)
 * Minor Improvements: Nicholas Miell (nmiell@gmail.com)
 * MMX code paths for fbcompose.c by Lars Knoll (lars@trolltech.com) 
 *
 * Based on work by Owen Taylor
 */


#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#ifdef USE_MMX

#if defined(__amd64__) || defined(__x86_64__)
#define USE_SSE
#endif

#include <mmintrin.h>
#include <xmmintrin.h> /* for _mm_shuffle_pi16 and _MM_SHUFFLE */

#ifdef RENDER

#include "fb.h"
#include "fbmmx.h"

#include "picturestr.h"
#include "mipict.h"
#include "fbpict.h"

#define noVERBOSE

#ifdef VERBOSE
#define CHECKPOINT() ErrorF ("at %s %d\n", __FUNCTION__, __LINE__)
#else
#define CHECKPOINT()
#endif

/* Notes about writing mmx code
 *
 * give memory operands as the second operand. If you give it as the
 * first, gcc will first load it into a register, then use that
 * register
 *
 *   ie. use
 *
 *         _mm_mullo_pi16 (x, mmx_constant);
 *
 *   not
 *
 *         _mm_mullo_pi16 (mmx_constant, x);
 *
 * Also try to minimize dependencies. i.e. when you need a value, try
 * to calculate it from a value that was calculated as early as
 * possible.
 */

/* --------------- MMX primitivess ------------------------------------ */

typedef unsigned long long ullong;

typedef struct
{
    ullong mmx_4x00ff;
    ullong mmx_4x0080;
    ullong mmx_565_rgb;
    ullong mmx_565_unpack_multiplier;
    ullong mmx_565_r;
    ullong mmx_565_g;
    ullong mmx_565_b;
    ullong mmx_mask_0;
    ullong mmx_mask_1;
    ullong mmx_mask_2;
    ullong mmx_mask_3;
    ullong mmx_full_alpha;
    ullong mmx_ffff0000ffff0000;
    ullong mmx_0000ffff00000000;
    ullong mmx_000000000000ffff;
} MMXData;

static const MMXData c =
{
    .mmx_4x00ff =			0x00ff00ff00ff00ffULL,
    .mmx_4x0080 =			0x0080008000800080ULL,
    .mmx_565_rgb =			0x000001f0003f001fULL,
    .mmx_565_r =			0x000000f800000000ULL,
    .mmx_565_g =			0x0000000000fc0000ULL,
    .mmx_565_b =			0x00000000000000f8ULL,
    .mmx_mask_0 =			0xffffffffffff0000ULL,
    .mmx_mask_1 =			0xffffffff0000ffffULL,
    .mmx_mask_2 =			0xffff0000ffffffffULL,
    .mmx_mask_3 =			0x0000ffffffffffffULL,
    .mmx_full_alpha =			0x00ff000000000000ULL,
    .mmx_565_unpack_multiplier =	0x0000008404100840ULL,
    .mmx_ffff0000ffff0000 =		0xffff0000ffff0000ULL,
    .mmx_0000ffff00000000 =		0x0000ffff00000000ULL,
    .mmx_000000000000ffff =		0x000000000000ffffULL,
};

#define MC(x) ((__m64) c.mmx_##x)

static __inline__ __m64
shift (__m64 v, int s)
{
    if (s > 0)
	return _mm_slli_si64 (v, s);
    else if (s < 0)
	return _mm_srli_si64 (v, -s);
    else
	return v;
}

static __inline__ __m64
negate (__m64 mask)
{
    return _mm_xor_si64 (mask, MC(4x00ff));
}

static __inline__ __m64
pix_multiply (__m64 a, __m64 b)
{
    __m64 res;
    
    res = _mm_mullo_pi16 (a, b);
    res = _mm_adds_pu16 (res, MC(4x0080));
    res = _mm_adds_pu16 (res, _mm_srli_pi16 (res, 8));
    res = _mm_srli_pi16 (res, 8);
    
    return res;
}

static __inline__ __m64
pix_add (__m64 a, __m64 b)
{
    return  _mm_adds_pu8 (a, b);
}

#ifdef USE_SSE

static __inline__ __m64
expand_alpha (__m64 pixel)
{
    return _mm_shuffle_pi16 (pixel, _MM_SHUFFLE(3, 3, 3, 3));
}

static __inline__ __m64
expand_alpha_rev (__m64 pixel)
{
    return _mm_shuffle_pi16 (pixel, _MM_SHUFFLE(0, 0, 0, 0));
}    

static __inline__ __m64
invert_colors (__m64 pixel)
{
    return _mm_shuffle_pi16 (pixel, _MM_SHUFFLE(3, 0, 1, 2));
}

#else

static __inline__ __m64
expand_alpha (__m64 pixel)
{
    __m64 t1, t2;
    
    t1 = shift (pixel, -48);
    t2 = shift (t1, 16);
    t1 = _mm_or_si64 (t1, t2);
    t2 = shift (t1, 32);
    t1 = _mm_or_si64 (t1, t2);

    return t1;
}

static __inline__ __m64
expand_alpha_rev (__m64 pixel)
{
    __m64 t1, t2;

    /* move alpha to low 16 bits and zero the rest */
    t1 = shift (pixel,  48);
    t1 = shift (t1, -48);

    t2 = shift (t1, 16);
    t1 = _mm_or_si64 (t1, t2);
    t2 = shift (t1, 32);
    t1 = _mm_or_si64 (t1, t2);

    return t1;
}

static __inline__ __m64
invert_colors (__m64 pixel)
{
    __m64 x, y, z;

    x = y = z = pixel;

    x = _mm_and_si64 (x, MC(ffff0000ffff0000));
    y = _mm_and_si64 (y, MC(000000000000ffff));
    z = _mm_and_si64 (z, MC(0000ffff00000000));

    y = shift (y, 32);
    z = shift (z, -32);

    x = _mm_or_si64 (x, y);
    x = _mm_or_si64 (x, z);

    return x;
}

#endif

static __inline__ __m64
over (__m64 src, __m64 srca, __m64 dest)
{
    return  _mm_adds_pu8 (src, pix_multiply(dest, negate(srca)));
}

static __inline__ __m64
over_rev_non_pre (__m64 src, __m64 dest)
{
    __m64 srca = expand_alpha (src);
    __m64 srcfaaa = _mm_or_si64 (srca, MC(full_alpha));
    
    return over(pix_multiply(invert_colors(src), srcfaaa), srca, dest);
}

static __inline__ __m64
in (__m64 src,
    __m64 mask)
{
    return pix_multiply (src, mask);
}

static __inline__ __m64
in_over (__m64 src,
	 __m64 srca,
	 __m64 mask,
	 __m64 dest)
{
    return over(in(src, mask), pix_multiply(srca, mask), dest);
}

static __inline__ __m64
load8888 (CARD32 v)
{
    return _mm_unpacklo_pi8 (_mm_cvtsi32_si64 (v), _mm_setzero_si64());
}

static __inline__ __m64
pack8888 (__m64 lo, __m64 hi)
{
    return _mm_packs_pu16 (lo, hi);
}

static __inline__ CARD32
store8888 (__m64 v)
{
    return _mm_cvtsi64_si32(pack8888(v, _mm_setzero_si64()));
}

/* Expand 16 bits positioned at @pos (0-3) of a mmx register into
 *
 *    00RR00GG00BB
 * 
 * --- Expanding 565 in the low word ---
 * 
 * m = (m << (32 - 3)) | (m << (16 - 5)) | m;
 * m = m & (01f0003f001f);
 * m = m * (008404100840);
 * m = m >> 8;
 * 
 * Note the trick here - the top word is shifted by another nibble to
 * avoid it bumping into the middle word
 */
static __inline__ __m64
expand565 (__m64 pixel, int pos)
{
    __m64 p = pixel;
    __m64 t1, t2;
    
    /* move pixel to low 16 bit and zero the rest */
    p = shift (shift (p, (3 - pos) * 16), -48); 
    
    t1 = shift (p, 36 - 11);
    t2 = shift (p, 16 - 5);
    
    p = _mm_or_si64 (t1, p);
    p = _mm_or_si64 (t2, p);
    p = _mm_and_si64 (p, MC(565_rgb));
    
    pixel = _mm_mullo_pi16 (p, MC(565_unpack_multiplier));
    return _mm_srli_pi16 (pixel, 8);
}

static __inline__ __m64
expand8888 (__m64 in, int pos)
{
    if (pos == 0)
	return _mm_unpacklo_pi8 (in, _mm_setzero_si64());
    else
	return _mm_unpackhi_pi8 (in, _mm_setzero_si64());
}

static __inline__ __m64
pack565 (__m64 pixel, __m64 target, int pos)
{
    __m64 p = pixel;
    __m64 t = target;
    __m64 r, g, b;
    
    r = _mm_and_si64 (p, MC(565_r));
    g = _mm_and_si64 (p, MC(565_g));
    b = _mm_and_si64 (p, MC(565_b));
    
    r = shift (r, - (32 - 8) + pos * 16);
    g = shift (g, - (16 - 3) + pos * 16);
    b = shift (b, - (0  + 3) + pos * 16);
    
    if (pos == 0)
	t = _mm_and_si64 (t, MC(mask_0));
    else if (pos == 1)
	t = _mm_and_si64 (t, MC(mask_1));
    else if (pos == 2)
	t = _mm_and_si64 (t, MC(mask_2));
    else if (pos == 3)
	t = _mm_and_si64 (t, MC(mask_3));
    
    p = _mm_or_si64 (r, t);
    p = _mm_or_si64 (g, p);
    
    return _mm_or_si64 (b, p);
}

static __inline__ __m64
pix_add_mul (__m64 x, __m64 a, __m64 y, __m64 b)
{
    x = _mm_mullo_pi16 (x, a);                  
    y = _mm_mullo_pi16 (y, b);                  
    x = _mm_srli_pi16(x, 1);                    
    y = _mm_srli_pi16(y, 1);                    
    x = _mm_adds_pu16 (x, y);                    
    x = _mm_adds_pu16 (x, _mm_srli_pi16 (x, 8)); 
    x = _mm_adds_pu16 (x, MC(4x0080));
    x = _mm_srli_pi16 (x, 7);

    return x;
}

/* --------------- MMX code patch for fbcompose.c --------------------- */

static FASTCALL void
mmxCombineMaskU (CARD32 *src, const CARD32 *mask, int width)
{
    const CARD32 *end = mask + width;
    while (mask < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        a = expand_alpha(a);
        s = pix_multiply(s, a);
        *src = store8888(s);
        ++src;
        ++mask;
    }
    _mm_empty();
}


static FASTCALL void
mmxCombineOverU (CARD32 *dest, const CARD32 *src, int width)
{
    const CARD32 *end = dest + width;

    while (dest < end) {
        __m64 s, sa;
	s = load8888(*src);
	sa = expand_alpha(s);
	*dest = store8888(over(s, sa, load8888(*dest)));
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineOverReverseU (CARD32 *dest, const CARD32 *src, int width)
{
    const CARD32 *end = dest + width;

    while (dest < end) {
	__m64 d, da;
	d = load8888(*dest);
	da = expand_alpha(d);
	*dest = store8888(over (d, da, load8888(*src)));
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineInU (CARD32 *dest, const CARD32 *src, int width)
{
    const CARD32 *end = dest + width;

    while (dest < end) {
        __m64 x, a;
        x = load8888(*src);
        a = load8888(*dest);
        a = expand_alpha(a);
        x = pix_multiply(x, a);
        *dest = store8888(x);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineInReverseU (CARD32 *dest, const CARD32 *src, int width)
{
    const CARD32 *end = dest + width;

    while (dest < end) {
        __m64 x, a;
        x = load8888(*dest);
        a = load8888(*src);
        a = expand_alpha(a);
        x = pix_multiply(x, a);
        *dest = store8888(x);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineOutU (CARD32 *dest, const CARD32 *src, int width)
{
    const CARD32 *end = dest + width;

    while (dest < end) {
        __m64 x, a;
        x = load8888(*src);
        a = load8888(*dest);
        a = expand_alpha(a);
        a = negate(a);
        x = pix_multiply(x, a);
        *dest = store8888(x);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineOutReverseU (CARD32 *dest, const CARD32 *src, int width)
{
    const CARD32 *end = dest + width;

    while (dest < end) {
        __m64 x, a;
        x = load8888(*dest);
        a = load8888(*src);
        a = expand_alpha(a);
        a = negate(a);
        x = pix_multiply(x, a);
        *dest = store8888(x);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineAtopU (CARD32 *dest, const CARD32 *src, int width)
{
    const CARD32 *end = dest + width;

    while (dest < end) {
        __m64 s, da, d, sia;
        s = load8888(*src);
        d = load8888(*dest);
        sia = expand_alpha(s);
        sia = negate(sia);
        da = expand_alpha(d);
        s = pix_add_mul (s, da, d, sia);
        *dest = store8888(s);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineAtopReverseU (CARD32 *dest, const CARD32 *src, int width)
{
    const CARD32 *end;

    end = dest + width;

    while (dest < end) {
        __m64 s, dia, d, sa;
        s = load8888(*src);
        d = load8888(*dest);
        sa = expand_alpha(s);
        dia = expand_alpha(d);
        dia = negate(dia);
	s = pix_add_mul (s, dia, d, sa);
        *dest = store8888(s);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineXorU (CARD32 *dest, const CARD32 *src, int width)
{
    const CARD32 *end = dest + width;

    while (dest < end) {
        __m64 s, dia, d, sia;
        s = load8888(*src);
        d = load8888(*dest);
        sia = expand_alpha(s);
        dia = expand_alpha(d);
        sia = negate(sia);
        dia = negate(dia);
	s = pix_add_mul (s, dia, d, sia);
        *dest = store8888(s);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineAddU (CARD32 *dest, const CARD32 *src, int width)
{
    const CARD32 *end = dest + width;
    while (dest < end) {
        __m64 s, d;
        s = load8888(*src);
        d = load8888(*dest);
        s = pix_add(s, d);
        *dest = store8888(s);
        ++dest;
        ++src;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineSaturateU (CARD32 *dest, const CARD32 *src, int width)
{
    const CARD32 *end = dest + width;
    while (dest < end) {
        CARD32 s = *src;
        CARD32 d = *dest;
        __m64 ms = load8888(s);
        __m64 md = load8888(d);
        CARD32 sa = s >> 24;
        CARD32 da = ~d >> 24;

        if (sa > da) {
            __m64 msa = load8888(FbIntDiv(da, sa));
            msa = expand_alpha(msa);
            ms = pix_multiply(ms, msa);
        }
        md = pix_add(md, ms);
        *dest = store8888(md);
        ++src;
        ++dest;
    }
    _mm_empty();
}


static FASTCALL void
mmxCombineSrcC (CARD32 *dest, CARD32 *src, CARD32 *mask, int width)
{
    const CARD32 *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        s = pix_multiply(s, a);
        *dest = store8888(s);
        ++src;
        ++mask;
        ++dest;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineOverC (CARD32 *dest, CARD32 *src, CARD32 *mask, int width)
{
    const CARD32 *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 sa = expand_alpha(s);
	
	*dest = store8888(in_over (s, sa, a, d));
	
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineOverReverseC (CARD32 *dest, CARD32 *src, CARD32 *mask, int width)
{
    const CARD32 *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 da = expand_alpha(d);

	*dest = store8888(over (d, da, in (s, a)));
	
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}


static FASTCALL void
mmxCombineInC (CARD32 *dest, CARD32 *src, CARD32 *mask, int width)
{
    const CARD32 *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 da = expand_alpha(d);
        s = pix_multiply(s, a);
        s = pix_multiply(s, da);
        *dest = store8888(s);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineInReverseC (CARD32 *dest, CARD32 *src, CARD32 *mask, int width)
{
    const CARD32 *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 sa = expand_alpha(s);
        a = pix_multiply(a, sa);
        d = pix_multiply(d, a);
        *dest = store8888(d);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineOutC (CARD32 *dest, CARD32 *src, CARD32 *mask, int width)
{
    const CARD32 *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 da = expand_alpha(d);
        da = negate(da);
        s = pix_multiply(s, a);
        s = pix_multiply(s, da);
        *dest = store8888(s);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineOutReverseC (CARD32 *dest, CARD32 *src, CARD32 *mask, int width)
{
    const CARD32 *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 sa = expand_alpha(s);
        a = pix_multiply(a, sa);
        a = negate(a);
        d = pix_multiply(d, a);
        *dest = store8888(d);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineAtopC (CARD32 *dest, CARD32 *src, CARD32 *mask, int width)
{
    const CARD32 *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 da = expand_alpha(d);
        __m64 sa = expand_alpha(s); 
        s = pix_multiply(s, a);
        a = pix_multiply(a, sa);
        a = negate(a);
	d = pix_add_mul (d, a, s, da);
        *dest = store8888(d);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineAtopReverseC (CARD32 *dest, CARD32 *src, CARD32 *mask, int width)
{
    const CARD32 *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 da = expand_alpha(d);
        __m64 sa = expand_alpha(s);
        s = pix_multiply(s, a);
        a = pix_multiply(a, sa);
        da = negate(da);
	d = pix_add_mul (d, a, s, da);
        *dest = store8888(d);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineXorC (CARD32 *dest, CARD32 *src, CARD32 *mask, int width)
{
    const CARD32 *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        __m64 da = expand_alpha(d);
        __m64 sa = expand_alpha(s);
        s = pix_multiply(s, a);
        a = pix_multiply(a, sa);
        da = negate(da);
        a = negate(a);
	d = pix_add_mul (d, a, s, da);
        *dest = store8888(d);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

static FASTCALL void
mmxCombineAddC (CARD32 *dest, CARD32 *src, CARD32 *mask, int width)
{
    const CARD32 *end = src + width;
    while (src < end) {
        __m64 a = load8888(*mask);
        __m64 s = load8888(*src);
        __m64 d = load8888(*dest);
        s = pix_multiply(s, a);
        d = pix_add(s, d);
        *dest = store8888(d);
        ++src;
        ++dest;
        ++mask;
    }
    _mm_empty();
}

extern FbComposeFunctions composeFunctions;

void fbComposeSetupMMX(void)
{
    /* check if we have MMX support and initialize accordingly */
    if (fbHaveMMX()) {
        composeFunctions.combineU[PictOpOver] = mmxCombineOverU;
        composeFunctions.combineU[PictOpOverReverse] = mmxCombineOverReverseU;
        composeFunctions.combineU[PictOpIn] = mmxCombineInU;
        composeFunctions.combineU[PictOpInReverse] = mmxCombineInReverseU;
        composeFunctions.combineU[PictOpOut] = mmxCombineOutU;
        composeFunctions.combineU[PictOpOutReverse] = mmxCombineOutReverseU;
        composeFunctions.combineU[PictOpAtop] = mmxCombineAtopU;
        composeFunctions.combineU[PictOpAtopReverse] = mmxCombineAtopReverseU;
        composeFunctions.combineU[PictOpXor] = mmxCombineXorU;
        composeFunctions.combineU[PictOpAdd] = mmxCombineAddU;
        composeFunctions.combineU[PictOpSaturate] = mmxCombineSaturateU;

        composeFunctions.combineC[PictOpSrc] = mmxCombineSrcC;
        composeFunctions.combineC[PictOpOver] = mmxCombineOverC;
        composeFunctions.combineC[PictOpOverReverse] = mmxCombineOverReverseC;
        composeFunctions.combineC[PictOpIn] = mmxCombineInC;
        composeFunctions.combineC[PictOpInReverse] = mmxCombineInReverseC;
        composeFunctions.combineC[PictOpOut] = mmxCombineOutC;
        composeFunctions.combineC[PictOpOutReverse] = mmxCombineOutReverseC;
        composeFunctions.combineC[PictOpAtop] = mmxCombineAtopC;
        composeFunctions.combineC[PictOpAtopReverse] = mmxCombineAtopReverseC;
        composeFunctions.combineC[PictOpXor] = mmxCombineXorC;
        composeFunctions.combineC[PictOpAdd] = mmxCombineAddC;

        composeFunctions.combineMaskU = mmxCombineMaskU;
    } 
}


/* ------------------ MMX code paths called from fbpict.c ----------------------- */

void
fbCompositeSolid_nx8888mmx (CARD8	op,
			    PicturePtr pSrc,
			    PicturePtr pMask,
			    PicturePtr pDst,
			    INT16	xSrc,
			    INT16	ySrc,
			    INT16	xMask,
			    INT16	yMask,
			    INT16	xDst,
			    INT16	yDst,
			    CARD16	width,
			    CARD16	height)
{
    CARD32	src;
    CARD32	*dstLine, *dst;
    CARD16	w;
    FbStride	dstStride;
    __m64	vsrc, vsrca;
    
    CHECKPOINT();
    
    fbComposeGetSolid(pSrc, src, pDst->format);
    
    if (src >> 24 == 0)
	return;
    
    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    
    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	w = width;
	
	CHECKPOINT();
	
	while (w && (unsigned long)dst & 7)
	{
	    *dst = store8888(over(vsrc, vsrca, load8888(*dst)));
	    
	    w--;
	    dst++;
	}
	
	while (w >= 2)
	{
	    __m64 vdest;
	    __m64 dest0, dest1;
	    
	    vdest = *(__m64 *)dst;
	    
	    dest0 = over(vsrc, vsrca, expand8888(vdest, 0));
	    dest1 = over(vsrc, vsrca, expand8888(vdest, 1));
	    
	    *(__m64 *)dst = pack8888(dest0, dest1);
	    
	    dst += 2;
	    w -= 2;
	}
	
	CHECKPOINT();
	
	while (w)
	{
	    *dst = store8888(over(vsrc, vsrca, load8888(*dst)));
	    
	    w--;
	    dst++;
	}
    }
    
    _mm_empty();
}

void
fbCompositeSolid_nx0565mmx (CARD8	op,
			    PicturePtr pSrc,
			    PicturePtr pMask,
			    PicturePtr pDst,
			    INT16	xSrc,
			    INT16	ySrc,
			    INT16	xMask,
			    INT16	yMask,
			    INT16	xDst,
			    INT16	yDst,
			    CARD16	width,
			    CARD16	height)
{
    CARD32	src;
    CARD16	*dstLine, *dst;
    CARD16	w;
    FbStride	dstStride;
    __m64	vsrc, vsrca;
    
    CHECKPOINT();
    
    fbComposeGetSolid(pSrc, src, pDst->format);
    
    if (src >> 24 == 0)
	return;
    
    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);
    
    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	w = width;
	
	CHECKPOINT();
	
	while (w && (unsigned long)dst & 7)
	{
	    ullong d = *dst;
	    __m64 vdest = expand565 ((__m64)d, 0);
	    vdest = pack565(over(vsrc, vsrca, vdest), vdest, 0);
	    *dst = (ullong)vdest;
	    
	    w--;
	    dst++;
	}
	
	while (w >= 4)
	{
	    __m64 vdest;
	    
	    vdest = *(__m64 *)dst;
	    
	    vdest = pack565 (over(vsrc, vsrca, expand565(vdest, 0)), vdest, 0);
	    vdest = pack565 (over(vsrc, vsrca, expand565(vdest, 1)), vdest, 1);
	    vdest = pack565 (over(vsrc, vsrca, expand565(vdest, 2)), vdest, 2);
	    vdest = pack565 (over(vsrc, vsrca, expand565(vdest, 3)), vdest, 3);
	    
	    *(__m64 *)dst = vdest;
	    
	    dst += 4;
	    w -= 4;
	}
	
	CHECKPOINT();
	
	while (w)
	{
	    ullong d = *dst;
	    __m64 vdest = expand565 ((__m64)d, 0);
	    vdest = pack565(over(vsrc, vsrca, vdest), vdest, 0);
	    *dst = (ullong)vdest;
	    
	    w--;
	    dst++;
	}
    }
    
    _mm_empty();
}

void
fbCompositeSolidMask_nx8888x8888Cmmx (CARD8	op,
				      PicturePtr pSrc,
				      PicturePtr pMask,
				      PicturePtr pDst,
				      INT16	xSrc,
				      INT16	ySrc,
				      INT16	xMask,
				      INT16	yMask,
				      INT16	xDst,
				      INT16	yDst,
				      CARD16	width,
				      CARD16	height)
{
    CARD32	src, srca;
    CARD32	*dstLine;
    CARD32	*maskLine;
    FbStride	dstStride, maskStride;
    __m64	vsrc, vsrca;
    
    CHECKPOINT();
    
    fbComposeGetSolid(pSrc, src, pDst->format);
    
    srca = src >> 24;
    if (srca == 0)
	return;
    
    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD32, maskStride, maskLine, 1);
    
    vsrc = load8888(src);
    vsrca = expand_alpha(vsrc);
    
    while (height--)
    {
	int twidth = width;
	CARD32 *p = (CARD32 *)maskLine;
	CARD32 *q = (CARD32 *)dstLine;
	
	while (twidth && (unsigned long)q & 7)
	{
	    CARD32 m = *(CARD32 *)p;
	    
	    if (m)
	    {
		__m64 vdest = load8888(*q);
		vdest = in_over(vsrc, vsrca, load8888(m), vdest);
		*q = store8888(vdest);
	    }
	    
	    twidth--;
	    p++;
	    q++;
	}
	
	while (twidth >= 2)
	{
	    CARD32 m0, m1;
	    m0 = *p;
	    m1 = *(p + 1);
	    
	    if (m0 | m1)
	    {
		__m64 dest0, dest1;
		__m64 vdest = *(__m64 *)q;
		
		dest0 = in_over(vsrc, vsrca, load8888(m0),
				expand8888 (vdest, 0));
		dest1 = in_over(vsrc, vsrca, load8888(m1),
				expand8888 (vdest, 1));
		
		*(__m64 *)q = pack8888(dest0, dest1);
	    }
	    
	    p += 2;
	    q += 2;
	    twidth -= 2;
	}
	
	while (twidth)
	{
	    CARD32 m = *(CARD32 *)p;
	    
	    if (m)
	    {
		__m64 vdest = load8888(*q);
		vdest = in_over(vsrc, vsrca, load8888(m), vdest);
		*q = store8888(vdest);
	    }
	    
	    twidth--;
	    p++;
	    q++;
	}
	
	dstLine += dstStride;
	maskLine += maskStride;
    }
    
    _mm_empty();
}

void
fbCompositeSrc_8888x8x8888mmx (CARD8	op,
			       PicturePtr pSrc,
			       PicturePtr pMask,
			       PicturePtr pDst,
			       INT16	xSrc,
			       INT16	ySrc,
			       INT16      xMask,
			       INT16      yMask,
			       INT16      xDst,
			       INT16      yDst,
			       CARD16     width,
			       CARD16     height)
{
    CARD32	*dstLine, *dst;
    CARD32	*srcLine, *src;
    CARD8	*maskLine;
    CARD32	mask;
    __m64	vmask;
    FbStride	dstStride, srcStride, maskStride;
    CARD16	w;
    __m64  srca;
    
    CHECKPOINT();
    
    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD8, maskStride, maskLine, 1);

    mask = *maskLine << 24 | *maskLine << 16 | *maskLine << 8 | *maskLine;
    vmask = load8888 (mask);
    srca = MC(4x00ff);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w && (unsigned long)dst & 7)
	{
	    __m64 s = load8888 (*src);
	    __m64 d = load8888 (*dst);
	    
	    *dst = store8888 (in_over (s, srca, vmask, d));
	    
	    w--;
	    dst++;
	    src++;
	}

	while (w >= 16)
	{
	    __m64 vd0 = *(__m64 *)(dst + 0);
	    __m64 vd1 = *(__m64 *)(dst + 2);
	    __m64 vd2 = *(__m64 *)(dst + 4);
	    __m64 vd3 = *(__m64 *)(dst + 6);
	    __m64 vd4 = *(__m64 *)(dst + 8);
	    __m64 vd5 = *(__m64 *)(dst + 10);
	    __m64 vd6 = *(__m64 *)(dst + 12);
	    __m64 vd7 = *(__m64 *)(dst + 14);

	    __m64 vs0 = *(__m64 *)(src + 0);
	    __m64 vs1 = *(__m64 *)(src + 2);
	    __m64 vs2 = *(__m64 *)(src + 4);
	    __m64 vs3 = *(__m64 *)(src + 6);
	    __m64 vs4 = *(__m64 *)(src + 8);
	    __m64 vs5 = *(__m64 *)(src + 10);
	    __m64 vs6 = *(__m64 *)(src + 12);
	    __m64 vs7 = *(__m64 *)(src + 14);

	    vd0 = (__m64)pack8888 (
		in_over (expand8888 (vs0, 0), srca, vmask, expand8888 (vd0, 0)),
		in_over (expand8888 (vs0, 1), srca, vmask, expand8888 (vd0, 1)));
	
	    vd1 = (__m64)pack8888 (
		in_over (expand8888 (vs1, 0), srca, vmask, expand8888 (vd1, 0)),
		in_over (expand8888 (vs1, 1), srca, vmask, expand8888 (vd1, 1)));
	
	    vd2 = (__m64)pack8888 (
		in_over (expand8888 (vs2, 0), srca, vmask, expand8888 (vd2, 0)),
		in_over (expand8888 (vs2, 1), srca, vmask, expand8888 (vd2, 1)));
	
	    vd3 = (__m64)pack8888 (
		in_over (expand8888 (vs3, 0), srca, vmask, expand8888 (vd3, 0)),
		in_over (expand8888 (vs3, 1), srca, vmask, expand8888 (vd3, 1)));
	
	    vd4 = (__m64)pack8888 (
		in_over (expand8888 (vs4, 0), srca, vmask, expand8888 (vd4, 0)),
		in_over (expand8888 (vs4, 1), srca, vmask, expand8888 (vd4, 1)));
	
	    vd5 = (__m64)pack8888 (
		in_over (expand8888 (vs5, 0), srca, vmask, expand8888 (vd5, 0)),
		in_over (expand8888 (vs5, 1), srca, vmask, expand8888 (vd5, 1)));
	
	    vd6 = (__m64)pack8888 (
		in_over (expand8888 (vs6, 0), srca, vmask, expand8888 (vd6, 0)),
		in_over (expand8888 (vs6, 1), srca, vmask, expand8888 (vd6, 1)));
	
	    vd7 = (__m64)pack8888 (
		in_over (expand8888 (vs7, 0), srca, vmask, expand8888 (vd7, 0)),
		in_over (expand8888 (vs7, 1), srca, vmask, expand8888 (vd7, 1)));

    	    *(__m64 *)(dst + 0) = vd0;
	    *(__m64 *)(dst + 2) = vd1;
	    *(__m64 *)(dst + 4) = vd2;
	    *(__m64 *)(dst + 6) = vd3;
	    *(__m64 *)(dst + 8) = vd4;
	    *(__m64 *)(dst + 10) = vd5;
	    *(__m64 *)(dst + 12) = vd6;
	    *(__m64 *)(dst + 14) = vd7;
	
	    w -= 16;
	    dst += 16;
	    src += 16;
	}
	
	while (w)
	{
	    __m64 s = load8888 (*src);
	    __m64 d = load8888 (*dst);
	    
	    *dst = store8888 (in_over (s, srca, vmask, d));
	    
	    w--;
	    dst++;
	    src++;
	}
    }

    _mm_empty(); 
}

void
fbCompositeSrc_8888x8888mmx (CARD8	op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
			     INT16	xSrc,
			     INT16	ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    CARD32	*dstLine, *dst;
    CARD32	*srcLine, *src;
    FbStride	dstStride, srcStride;
    CARD16	w;
    __m64  srca;
    
    CHECKPOINT();
    
    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);

    srca = MC (4x00ff);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w && (unsigned long)dst & 7)
	{
	    __m64 s = load8888 (*src);
	    __m64 d = load8888 (*dst);
	    
	    *dst = store8888 (over (s, expand_alpha (s), d));
	    
	    w--;
	    dst++;
	    src++;
	}

	while (w >= 2)
	{
	    __m64 vd = *(__m64 *)(dst + 0);
	    __m64 vs = *(__m64 *)(src + 0);
	    __m64 vs0 = expand8888 (vs, 0);
	    __m64 vs1 = expand8888 (vs, 1);

	    *(__m64 *)dst = (__m64)pack8888 (
		over (vs0, expand_alpha (vs0), expand8888 (vd, 0)),
		over (vs1, expand_alpha (vs1), expand8888 (vd, 1)));
	    
	    w -= 2;
	    dst += 2;
	    src += 2;
	}
	
	while (w)
	{
	    __m64 s = load8888 (*src);
	    __m64 d = load8888 (*dst);
	    
	    *dst = store8888 (over (s, expand_alpha (s), d));
	    
	    w--;
	    dst++;
	    src++;
	}
    }

    _mm_empty(); 
}

void
fbCompositeSolidMask_nx8x8888mmx (CARD8      op,
				  PicturePtr pSrc,
				  PicturePtr pMask,
				  PicturePtr pDst,
				  INT16      xSrc,
				  INT16      ySrc,
				  INT16      xMask,
				  INT16      yMask,
				  INT16      xDst,
				  INT16      yDst,
				  CARD16     width,
				  CARD16     height)
{
    CARD32	src, srca;
    CARD32	*dstLine, *dst;
    CARD8	*maskLine, *mask;
    FbStride	dstStride, maskStride;
    CARD16	w;
    __m64	vsrc, vsrca;
    ullong	srcsrc;
    
    CHECKPOINT();
    
    fbComposeGetSolid(pSrc, src, pDst->format);
    
    srca = src >> 24;
    if (srca == 0)
	return;
    
    srcsrc = (unsigned long long)src << 32 | src;
    
    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD8, maskStride, maskLine, 1);
    
    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;
	
	CHECKPOINT();
	
	while (w && (unsigned long)dst & 7)
	{
	    ullong m = *mask;
	    
	    if (m)
	    {
		__m64 vdest = in_over(vsrc, vsrca, expand_alpha_rev ((__m64)m), load8888(*dst));
		*dst = store8888(vdest);
	    }
	    
	    w--;
	    mask++;
	    dst++;
	}
	
	CHECKPOINT();
	
	while (w >= 2)
	{
	    ullong m0, m1;
	    m0 = *mask;
	    m1 = *(mask + 1);
	    
	    if (srca == 0xff && (m0 & m1) == 0xff)
	    {
		*(unsigned long long *)dst = srcsrc;
	    }
	    else if (m0 | m1)
	    {
		__m64 vdest;
		__m64 dest0, dest1;
		
		vdest = *(__m64 *)dst;
		
		dest0 = in_over(vsrc, vsrca, expand_alpha_rev ((__m64)m0), expand8888(vdest, 0));
		dest1 = in_over(vsrc, vsrca, expand_alpha_rev ((__m64)m1), expand8888(vdest, 1));
		
		*(__m64 *)dst = pack8888(dest0, dest1);
	    }
	    
	    mask += 2;
	    dst += 2;
	    w -= 2;
	}
	
	CHECKPOINT();
	
	while (w)
	{
	    ullong m = *mask;
	    
	    if (m)
	    {
		__m64 vdest = load8888(*dst);
		vdest = in_over(vsrc, vsrca, expand_alpha_rev ((__m64)m), vdest);
		*dst = store8888(vdest);
	    }
	    
	    w--;
	    mask++;
	    dst++;
	}
    }
    
    _mm_empty();
}


void
fbCompositeSolidMask_nx8x0565mmx (CARD8      op,
				  PicturePtr pSrc,
				  PicturePtr pMask,
				  PicturePtr pDst,
				  INT16      xSrc,
				  INT16      ySrc,
				  INT16      xMask,
				  INT16      yMask,
				  INT16      xDst,
				  INT16      yDst,
				  CARD16     width,
				  CARD16     height)
{
    CARD32	src, srca;
    CARD16	*dstLine, *dst;
    CARD8	*maskLine, *mask;
    FbStride	dstStride, maskStride;
    CARD16	w;
    __m64	vsrc, vsrca;
    unsigned long long srcsrcsrcsrc, src16;
    
    CHECKPOINT();
    
    fbComposeGetSolid(pSrc, src, pDst->format);
    
    srca = src >> 24;
    if (srca == 0)
	return;
    
    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD8, maskStride, maskLine, 1);
    
    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);
    
    src16 = (ullong)pack565(vsrc, _mm_setzero_si64(), 0);
    
    srcsrcsrcsrc = (ullong)src16 << 48 | (ullong)src16 << 32 |
	(ullong)src16 << 16 | (ullong)src16;
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;
	
	CHECKPOINT();
	
	while (w && (unsigned long)dst & 7)
	{
	    ullong m = *mask;
	    
	    if (m)
	    {
		ullong d = *dst;
		__m64 vd = (__m64)d;
		__m64 vdest = in_over(vsrc, vsrca, expand_alpha_rev ((__m64)m), expand565(vd, 0));
		*dst = (ullong)pack565(vdest, _mm_setzero_si64(), 0);
	    }
	    
	    w--;
	    mask++;
	    dst++;
	}
	
	CHECKPOINT();
	
	while (w >= 4)
	{
	    ullong m0, m1, m2, m3;
	    m0 = *mask;
	    m1 = *(mask + 1);
	    m2 = *(mask + 2);
	    m3 = *(mask + 3);
	    
	    if (srca == 0xff && (m0 & m1 & m2 & m3) == 0xff)
	    {
		*(unsigned long long *)dst = srcsrcsrcsrc;
	    }
	    else if (m0 | m1 | m2 | m3)
	    {
		__m64 vdest;
		__m64 vm0, vm1, vm2, vm3;
		
		vdest = *(__m64 *)dst;
		
		vm0 = (__m64)m0;
		vdest = pack565(in_over(vsrc, vsrca, expand_alpha_rev(vm0), expand565(vdest, 0)), vdest, 0);
		vm1 = (__m64)m1;
		vdest = pack565(in_over(vsrc, vsrca, expand_alpha_rev(vm1), expand565(vdest, 1)), vdest, 1);
		vm2 = (__m64)m2;
		vdest = pack565(in_over(vsrc, vsrca, expand_alpha_rev(vm2), expand565(vdest, 2)), vdest, 2);
		vm3 = (__m64)m3;
		vdest = pack565(in_over(vsrc, vsrca, expand_alpha_rev(vm3), expand565(vdest, 3)), vdest, 3);
		
		*(__m64 *)dst = vdest;
	    }
	    
	    w -= 4;
	    mask += 4;
	    dst += 4;
	}
	
	CHECKPOINT();
	
	while (w)
	{
	    ullong m = *mask;
	    
	    if (m)
	    {
		ullong d = *dst;
		__m64 vd = (__m64)d;
		__m64 vdest = in_over(vsrc, vsrca, expand_alpha_rev ((__m64)m), expand565(vd, 0));
		*dst = (ullong)pack565(vdest, _mm_setzero_si64(), 0);
	    }
	    
	    w--;
	    mask++;
	    dst++;
	}
    }
    
    _mm_empty();
}

void
fbCompositeSrc_8888RevNPx0565mmx (CARD8      op,
				  PicturePtr pSrc,
				  PicturePtr pMask,
				  PicturePtr pDst,
				  INT16      xSrc,
				  INT16      ySrc,
				  INT16      xMask,
				  INT16      yMask,
				  INT16      xDst,
				  INT16      yDst,
				  CARD16     width,
				  CARD16     height)
{
    CARD16	*dstLine, *dst;
    CARD32	*srcLine, *src;
    FbStride	dstStride, srcStride;
    CARD16	w;
    
    CHECKPOINT();
    
    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    
    assert (pSrc->pDrawable == pMask->pDrawable);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;
	
	CHECKPOINT();
	
	while (w && (unsigned long)dst & 7)
	{
	    __m64 vsrc = load8888 (*src);
	    ullong d = *dst;
	    __m64 vdest = expand565 ((__m64)d, 0);
	    
	    vdest = pack565(over_rev_non_pre(vsrc, vdest), vdest, 0);
	    
	    *dst = (ullong)vdest;
	    
	    w--;
	    dst++;
	    src++;
	}
	
	CHECKPOINT();
	
	while (w >= 4)
	{
	    CARD32 s0, s1, s2, s3;
	    unsigned char a0, a1, a2, a3;
	    
	    s0 = *src;
	    s1 = *(src + 1);
	    s2 = *(src + 2);
	    s3 = *(src + 3);
	    
	    a0 = (s0 >> 24);
	    a1 = (s1 >> 24);
	    a2 = (s2 >> 24);
	    a3 = (s3 >> 24);
	    
	    if ((a0 & a1 & a2 & a3) == 0xFF)
	    {
		__m64 vdest;
		vdest = pack565(invert_colors(load8888(s0)), _mm_setzero_si64(), 0);
		vdest = pack565(invert_colors(load8888(s1)), vdest, 1);
		vdest = pack565(invert_colors(load8888(s2)), vdest, 2);
		vdest = pack565(invert_colors(load8888(s3)), vdest, 3);
		
		*(__m64 *)dst = vdest;
	    }
	    else if (a0 | a1 | a2 | a3)
	    {
		__m64 vdest = *(__m64 *)dst;
		
		vdest = pack565(over_rev_non_pre(load8888(s0), expand565(vdest, 0)), vdest, 0);
	        vdest = pack565(over_rev_non_pre(load8888(s1), expand565(vdest, 1)), vdest, 1);
		vdest = pack565(over_rev_non_pre(load8888(s2), expand565(vdest, 2)), vdest, 2);
		vdest = pack565(over_rev_non_pre(load8888(s3), expand565(vdest, 3)), vdest, 3);
		
		*(__m64 *)dst = vdest;
	    }
	    
	    w -= 4;
	    dst += 4;
	    src += 4;
	}
	
	CHECKPOINT();
	
	while (w)
	{
	    __m64 vsrc = load8888 (*src);
	    ullong d = *dst;
	    __m64 vdest = expand565 ((__m64)d, 0);
	    
	    vdest = pack565(over_rev_non_pre(vsrc, vdest), vdest, 0);
	    
	    *dst = (ullong)vdest;
	    
	    w--;
	    dst++;
	    src++;
	}
    }
    
    _mm_empty();
}

/* "8888RevNP" is GdkPixbuf's format: ABGR, non premultiplied */

void
fbCompositeSrc_8888RevNPx8888mmx (CARD8      op,
				  PicturePtr pSrc,
				  PicturePtr pMask,
				  PicturePtr pDst,
				  INT16      xSrc,
				  INT16      ySrc,
				  INT16      xMask,
				  INT16      yMask,
				  INT16      xDst,
				  INT16      yDst,
				  CARD16     width,
				  CARD16     height)
{
    CARD32	*dstLine, *dst;
    CARD32	*srcLine, *src;
    FbStride	dstStride, srcStride;
    CARD16	w;
    
    CHECKPOINT();
    
    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    
    assert (pSrc->pDrawable == pMask->pDrawable);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;
	
	while (w && (unsigned long)dst & 7)
	{
	    __m64 s = load8888 (*src);
	    __m64 d = load8888 (*dst);
	    
	    *dst = store8888 (over_rev_non_pre (s, d));
	    
	    w--;
	    dst++;
	    src++;
	}
	
	while (w >= 2)
	{
	    ullong s0, s1;
	    unsigned char a0, a1;
	    __m64 d0, d1;
	    
	    s0 = *src;
	    s1 = *(src + 1);
	    
	    a0 = (s0 >> 24);
	    a1 = (s1 >> 24);
	    
	    if ((a0 & a1) == 0xFF)
	    {
		d0 = invert_colors(load8888(s0));
		d1 = invert_colors(load8888(s1));
		
		*(__m64 *)dst = pack8888 (d0, d1);
	    }
	    else if (a0 | a1)
	    {
		__m64 vdest = *(__m64 *)dst;
		
		d0 = over_rev_non_pre (load8888(s0), expand8888 (vdest, 0));
		d1 = over_rev_non_pre (load8888(s1), expand8888 (vdest, 1));
		
		*(__m64 *)dst = pack8888 (d0, d1);
	    }
	    
	    w -= 2;
	    dst += 2;
	    src += 2;
	}
	
	while (w)
	{
	    __m64 s = load8888 (*src);
	    __m64 d = load8888 (*dst);
	    
	    *dst = store8888 (over_rev_non_pre (s, d));
	    
	    w--;
	    dst++;
	    src++;
	}
    }
    
    _mm_empty();
}

void
fbCompositeSolidMask_nx8888x0565Cmmx (CARD8      op,
				      PicturePtr pSrc,
				      PicturePtr pMask,
				      PicturePtr pDst,
				      INT16      xSrc,
				      INT16      ySrc,
				      INT16      xMask,
				      INT16      yMask,
				      INT16      xDst,
				      INT16      yDst,
				      CARD16     width,
				      CARD16     height)
{
    CARD32	src, srca;
    CARD16	*dstLine;
    CARD32	*maskLine;
    FbStride	dstStride, maskStride;
    __m64  vsrc, vsrca;
    
    CHECKPOINT();
    
    fbComposeGetSolid(pSrc, src, pDst->format);
    
    srca = src >> 24;
    if (srca == 0)
	return;
    
    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD32, maskStride, maskLine, 1);
    
    vsrc = load8888 (src);
    vsrca = expand_alpha (vsrc);
    
    while (height--)
    {
	int twidth = width;
	CARD32 *p = (CARD32 *)maskLine;
	CARD16 *q = (CARD16 *)dstLine;
	
	while (twidth && ((unsigned long)q & 7))
	{
	    CARD32 m = *(CARD32 *)p;
	    
	    if (m)
	    {
		ullong d = *q;
		__m64 vdest = expand565 ((__m64)d, 0);
		vdest = pack565 (in_over (vsrc, vsrca, load8888 (m), vdest), vdest, 0);
		*q = (ullong)vdest;
	    }
	    
	    twidth--;
	    p++;
	    q++;
	}
	
	while (twidth >= 4)
	{
	    CARD32 m0, m1, m2, m3;
	    
	    m0 = *p;
	    m1 = *(p + 1);
	    m2 = *(p + 2);
	    m3 = *(p + 3);
	    
	    if ((m0 | m1 | m2 | m3))
	    {
		__m64 vdest = *(__m64 *)q;
		
		vdest = pack565(in_over(vsrc, vsrca, load8888(m0), expand565(vdest, 0)), vdest, 0);
		vdest = pack565(in_over(vsrc, vsrca, load8888(m1), expand565(vdest, 1)), vdest, 1);
		vdest = pack565(in_over(vsrc, vsrca, load8888(m2), expand565(vdest, 2)), vdest, 2);
		vdest = pack565(in_over(vsrc, vsrca, load8888(m3), expand565(vdest, 3)), vdest, 3);
		
		*(__m64 *)q = vdest;
	    }
	    twidth -= 4;
	    p += 4;
	    q += 4;
	}
	
	while (twidth)
	{
	    CARD32 m;
	    
	    m = *(CARD32 *)p;
	    if (m)
	    {
		ullong d = *q;
		__m64 vdest = expand565((__m64)d, 0);
		vdest = pack565 (in_over(vsrc, vsrca, load8888(m), vdest), vdest, 0);
		*q = (ullong)vdest;
	    }
	    
	    twidth--;
	    p++;
	    q++;
	}
	
	maskLine += maskStride;
	dstLine += dstStride;
    }
    
    _mm_empty ();
}

void
fbCompositeSrcAdd_8000x8000mmx (CARD8	op,
				PicturePtr pSrc,
				PicturePtr pMask,
				PicturePtr pDst,
				INT16      xSrc,
				INT16      ySrc,
				INT16      xMask,
				INT16      yMask,
				INT16      xDst,
				INT16      yDst,
				CARD16     width,
				CARD16     height)
{
    CARD8	*dstLine, *dst;
    CARD8	*srcLine, *src;
    FbStride	dstStride, srcStride;
    CARD16	w;
    CARD8	s, d;
    CARD16	t;
    
    CHECKPOINT();
    
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD8, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, CARD8, dstStride, dstLine, 1);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;
	
	while (w && (unsigned long)dst & 7)
	{
	    s = *src;
	    d = *dst;
	    t = d + s;
	    s = t | (0 - (t >> 8));
	    *dst = s;
	    
	    dst++;
	    src++;
	    w--;
	}
	
	while (w >= 8)
	{
	    *(__m64*)dst = _mm_adds_pu8(*(__m64*)src, *(__m64*)dst);
	    dst += 8;
	    src += 8;
	    w -= 8;
	}
	
	while (w)
	{
	    s = *src;
	    d = *dst;
	    t = d + s;
	    s = t | (0 - (t >> 8));
	    *dst = s;
	    
	    dst++;
	    src++;
	    w--;
	}
    }
    
    _mm_empty();
}

void
fbCompositeSrcAdd_8888x8888mmx (CARD8		op,
				PicturePtr	pSrc,
				PicturePtr	pMask,
				PicturePtr	 pDst,
				INT16		 xSrc,
				INT16      ySrc,
				INT16      xMask,
				INT16      yMask,
				INT16      xDst,
				INT16      yDst,
				CARD16     width,
				CARD16     height)
{
    CARD32	*dstLine, *dst;
    CARD32	*srcLine, *src;
    FbStride	dstStride, srcStride;
    CARD16	w;
    
    CHECKPOINT();
    
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;
	
	while (w && (unsigned long)dst & 7)
	{
	    *dst = _mm_cvtsi64_si32(_mm_adds_pu8(_mm_cvtsi32_si64(*src),
						 _mm_cvtsi32_si64(*dst)));
	    dst++;
	    src++;
	    w--;
	}
	
	while (w >= 2)
	{
	    *(ullong*)dst = (ullong) _mm_adds_pu8(*(__m64*)src, *(__m64*)dst);
	    dst += 2;
	    src += 2;
	    w -= 2;
	}
	
	if (w)
	{
	    *dst = _mm_cvtsi64_si32(_mm_adds_pu8(_mm_cvtsi32_si64(*src),
						 _mm_cvtsi32_si64(*dst)));
	    
	}
    }
    
    _mm_empty();
}

Bool
fbSolidFillmmx (DrawablePtr	pDraw,
		int		x,
		int		y,
		int		width,
		int		height,
		FbBits		xor)
{ 
    FbStride	stride;
    int		bpp;
    ullong	fill;
    __m64	vfill;
    CARD32	byte_width;
    CARD8	*byte_line;
    FbBits      *bits;
    int		xoff, yoff;
    __m64	v1, v2, v3, v4, v5, v6, v7;
    
    CHECKPOINT();
    
    fbGetDrawable(pDraw, bits, stride, bpp, xoff, yoff);
    
    if (bpp == 16 && (xor >> 16 != (xor & 0xffff)))
	return FALSE;
    
    if (bpp != 16 && bpp != 32)
	return FALSE;
    
    if (bpp == 16)
    {
	stride = stride * sizeof (FbBits) / 2;
	byte_line = (CARD8 *)(((CARD16 *)bits) + stride * (y + yoff) + (x + xoff));
	byte_width = 2 * width;
	stride *= 2;
    }
    else
    {
	stride = stride * sizeof (FbBits) / 4;
	byte_line = (CARD8 *)(((CARD32 *)bits) + stride * (y + yoff) + (x + xoff));
	byte_width = 4 * width;
	stride *= 4;
    }
    
    fill = ((ullong)xor << 32) | xor;
    vfill = (__m64)fill;
    
    __asm__ (
	"movq		%7,	%0\n"
	"movq		%7,	%1\n"
	"movq		%7,	%2\n"
	"movq		%7,	%3\n"
	"movq		%7,	%4\n"
	"movq		%7,	%5\n"
	"movq		%7,	%6\n"
	: "=y" (v1), "=y" (v2), "=y" (v3),
	  "=y" (v4), "=y" (v5), "=y" (v6), "=y" (v7)
	: "y" (vfill));
    
    while (height--)
    {
	int w;
	CARD8 *d = byte_line;
	byte_line += stride;
	w = byte_width;
	
	while (w >= 2 && ((unsigned long)d & 3))
	{
	    *(CARD16 *)d = xor;
	    w -= 2;
	    d += 2;
	}
	
	while (w >= 4 && ((unsigned long)d & 7))
	{
	    *(CARD32 *)d = xor;
	    
	    w -= 4;
	    d += 4;
	}

	while (w >= 64)
	{
	    __asm__ (
		"movq	%1,	  (%0)\n"
		"movq	%2,	 8(%0)\n"
		"movq	%3,	16(%0)\n"
		"movq	%4,	24(%0)\n"
		"movq	%5,	32(%0)\n"
		"movq	%6,	40(%0)\n"
		"movq	%7,	48(%0)\n"
		"movq	%8,	56(%0)\n"
		:
		: "r" (d),
		  "y" (vfill), "y" (v1), "y" (v2), "y" (v3),
		  "y" (v4), "y" (v5), "y" (v6), "y" (v7)
		: "memory");
	    
	    w -= 64;
	    d += 64;
	}
	
	while (w >= 4)
	{
	    *(CARD32 *)d = xor;
	    
	    w -= 4;
	    d += 4;
	}
	if (w >= 2)
	{
	    *(CARD16 *)d = xor;
	    w -= 2;
	    d += 2;
	}
    }
    
    _mm_empty();
    return TRUE;
}

Bool
fbCopyAreammx (DrawablePtr	pSrc,
	       DrawablePtr	pDst,
	       int		src_x,
	       int		src_y,
	       int		dst_x,
	       int		dst_y,
	       int		width,
	       int		height)
{
    FbBits *	src_bits;
    FbStride	src_stride;
    int		src_bpp;
    int		src_xoff;
    int		src_yoff;

    FbBits *	dst_bits;
    FbStride	dst_stride;
    int		dst_bpp;
    int		dst_xoff;
    int		dst_yoff;

    CARD8 *	src_bytes;
    CARD8 *	dst_bytes;
    int		byte_width;
    
    fbGetDrawable(pSrc, src_bits, src_stride, src_bpp, src_xoff, src_yoff);
    fbGetDrawable(pDst, dst_bits, dst_stride, dst_bpp, dst_xoff, dst_yoff);

    if (src_bpp != dst_bpp)
	return FALSE;
    
    if (src_bpp == 16)
    {
	src_stride = src_stride * sizeof (FbBits) / 2;
	dst_stride = dst_stride * sizeof (FbBits) / 2;
	src_bytes = (CARD8 *)(((CARD16 *)src_bits) + src_stride * (src_y + src_yoff) + (src_x + src_xoff));
	dst_bytes = (CARD8 *)(((CARD16 *)dst_bits) + dst_stride * (dst_y + dst_yoff) + (dst_x + dst_xoff));
	byte_width = 2 * width;
	src_stride *= 2;
	dst_stride *= 2;
    } else if (src_bpp == 32) {
	src_stride = src_stride * sizeof (FbBits) / 4;
	dst_stride = dst_stride * sizeof (FbBits) / 4;
	src_bytes = (CARD8 *)(((CARD32 *)src_bits) + src_stride * (src_y + src_yoff) + (src_x + src_xoff));
	dst_bytes = (CARD8 *)(((CARD32 *)dst_bits) + dst_stride * (dst_y + dst_yoff) + (dst_x + dst_xoff));
	byte_width = 4 * width;
	src_stride *= 4;
	dst_stride *= 4;
    } else {
	return FALSE;
    }

    while (height--)
    {
	int w;
	CARD8 *s = src_bytes;
	CARD8 *d = dst_bytes;
	src_bytes += src_stride;
	dst_bytes += dst_stride;
	w = byte_width;
	
	while (w >= 2 && ((unsigned long)d & 3))
	{
	    *(CARD16 *)d = *(CARD16 *)s;
	    w -= 2;
	    s += 2;
	    d += 2;
	}
	
	while (w >= 4 && ((unsigned long)d & 7))
	{
	    *(CARD32 *)d = *(CARD32 *)s;
	    
	    w -= 4;
	    s += 4;
	    d += 4;
	}
	
	while (w >= 64)
	{
	    __asm__ (
		"movq	  (%1),	  %%mm0\n"
		"movq	 8(%1),	  %%mm1\n"
		"movq	16(%1),	  %%mm2\n"
		"movq	24(%1),	  %%mm3\n"
		"movq	32(%1),	  %%mm4\n"
		"movq	40(%1),	  %%mm5\n"
		"movq	48(%1),	  %%mm6\n"
		"movq	56(%1),	  %%mm7\n"

		"movq	%%mm0,	  (%0)\n"
		"movq	%%mm1,	 8(%0)\n"
		"movq	%%mm2,	16(%0)\n"
		"movq	%%mm3,	24(%0)\n"
		"movq	%%mm4,	32(%0)\n"
		"movq	%%mm5,	40(%0)\n"
		"movq	%%mm6,	48(%0)\n"
		"movq	%%mm7,	56(%0)\n"
		:
		: "r" (d), "r" (s)
		: "memory",
		  "%mm0", "%mm1", "%mm2", "%mm3",
		  "%mm4", "%mm5", "%mm6", "%mm7");
	    
	    w -= 64;
	    s += 64;
	    d += 64;
	}
	while (w >= 4)
	{
	    *(CARD32 *)d = *(CARD32 *)s;

	    w -= 4;
	    s += 4;
	    d += 4;
	}
	if (w >= 2)
	{
	    *(CARD16 *)d = *(CARD16 *)s;
	    w -= 2;
	    s += 2;
	    d += 2;
	}
    }
    
    _mm_empty();
    return TRUE;
}

void
fbCompositeCopyAreammx (CARD8		op,
			PicturePtr	pSrc,
			PicturePtr	pMask,
			PicturePtr	pDst,
			INT16		xSrc,
			INT16		ySrc,
			INT16		xMask,
			INT16		yMask,
			INT16		xDst,
			INT16		yDst,
			CARD16		width,
			CARD16		height)
{
    fbCopyAreammx (pSrc->pDrawable,
		   pDst->pDrawable,
		   xSrc, ySrc,
		   xDst, yDst,
		   width, height);
}

typedef struct {
    ullong subYw;
    ullong U_green;
    ullong U_blue;
    ullong V_red;
    ullong V_green;
    ullong Y_coeff;
    ullong mmx0080;
    ullong mmx00ff;
} YUVData;

static const YUVData yuv = {
    .subYw   = 0x1010101010101010ULL,
    .U_green = 0xf377f377f377f377ULL,
    .U_blue  = 0x408d408d408d408dULL,
    .V_red   = 0x3313331333133313ULL,
    .V_green = 0xe5fce5fce5fce5fcULL,
    .Y_coeff = 0x2543254325432543ULL,
    .mmx0080 = 0x0080008000800080ULL,
    .mmx00ff = 0x00ff00ff00ff00ffULL
};

static __inline__ void
mmx_loadyv12 (CARD8 *py,
	      CARD8 *pu,
	      CARD8 *pv)
{
    __asm__ __volatile__ (
	"movq      %0,    %%mm6\n" /* mm6 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 */
	"pxor      %%mm4, %%mm4\n" /* mm4 = 0                       */
	"psubusb   %1,    %%mm6\n" /* Y -= 16                       */
	"movd      %2,    %%mm0\n" /* mm0 = 00 00 00 00 U3 U2 U1 U0 */
	"movq      %%mm6, %%mm7\n" /* mm7 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 */
	"pand      %3,    %%mm6\n" /* mm6 =    Y6    Y4    Y2    Y0 */
	"psrlw     %4,    %%mm7\n" /* mm7 =    Y7    Y5    Y3    Y1 */
	"movd      %5,    %%mm1\n" /* mm1 = 00 00 00 00 V3 V2 V1 V0 */
	"psllw     %6,    %%mm6\n" /* promote precision             */
	"pmulhw    %7,    %%mm6\n" /* mm6 = luma_rgb even           */
	"psllw     %8,    %%mm7\n" /* promote precision             */
	"punpcklbw %%mm4, %%mm0\n" /* mm0 = U3 U2 U1 U0             */
	"psubsw    %9,    %%mm0\n" /* U -= 128                      */
	"punpcklbw %%mm4, %%mm1\n" /* mm1 = V3 V2 V1 V0             */
	"pmulhw    %10,   %%mm7\n" /* mm7 = luma_rgb odd            */
	"psllw     %11,   %%mm0\n" /* promote precision             */
	"psubsw    %12,   %%mm1\n" /* V -= 128                      */
	"movq      %%mm0, %%mm2\n" /* mm2 = U3 U2 U1 U0             */
	"psllw     %13,   %%mm1\n" /* promote precision             */
	"movq      %%mm1, %%mm4\n" /* mm4 = V3 V2 V1 V0             */
	"pmulhw    %14,   %%mm0\n" /* mm0 = chroma_b                */
	"pmulhw    %15,   %%mm1\n" /* mm1 = chroma_r                */
	"movq      %%mm0, %%mm3\n" /* mm3 = chroma_b                */
	"paddsw    %%mm6, %%mm0\n" /* mm0 = B6 B4 B2 B0             */
	"paddsw    %%mm7, %%mm3\n" /* mm3 = B7 B5 B3 B1             */
	"packuswb  %%mm0, %%mm0\n" /* saturate to 0-255             */
	"pmulhw    %16,   %%mm2\n" /* mm2 = U * U_green             */
	"packuswb  %%mm3, %%mm3\n" /* saturate to 0-255             */
	"punpcklbw %%mm3, %%mm0\n" /* mm0 = B7 B6 B5 B4 B3 B2 B1 B0 */
	"pmulhw    %17,   %%mm4\n" /* mm4 = V * V_green             */
	"paddsw    %%mm4, %%mm2\n" /* mm2 = chroma_g                */
	"movq      %%mm2, %%mm5\n" /* mm5 = chroma_g                */
	"movq      %%mm1, %%mm4\n" /* mm4 = chroma_r                */
	"paddsw    %%mm6, %%mm2\n" /* mm2 = G6 G4 G2 G0             */
	"packuswb  %%mm2, %%mm2\n" /* saturate to 0-255             */
	"paddsw    %%mm6, %%mm1\n" /* mm1 = R6 R4 R2 R0             */
	"packuswb  %%mm1, %%mm1\n" /* saturate to 0-255             */
	"paddsw    %%mm7, %%mm4\n" /* mm4 = R7 R5 R3 R1             */
	"packuswb  %%mm4, %%mm4\n" /* saturate to 0-255             */
	"paddsw    %%mm7, %%mm5\n" /* mm5 = G7 G5 G3 G1             */
	"packuswb  %%mm5, %%mm5\n" /* saturate to 0-255             */
	"punpcklbw %%mm4, %%mm1\n" /* mm1 = R7 R6 R5 R4 R3 R2 R1 R0 */
	"punpcklbw %%mm5, %%mm2\n" /* mm2 = G7 G6 G5 G4 G3 G2 G1 G0 */
	: /* no outputs */
	: "m" (*py), "m" (yuv.subYw), "m" (*pu), "m" (yuv.mmx00ff),
	  "i" (8), "m" (*pv), "i" (3), "m" (yuv.Y_coeff),
	  "i" (3), "m" (yuv.mmx0080), "m" (yuv.Y_coeff), "i" (3),
	  "m" (yuv.mmx0080), "i" (3), "m" (yuv.U_blue), "m" (yuv.V_red),
	  "m" (yuv.U_green), "m" (yuv.V_green));
}

static __inline__ void
mmx_pack8888 (CARD8 *image)
{
    __asm__ __volatile__ (
	"pxor      %%mm3, %%mm3\n"
	"movq      %%mm0, %%mm6\n"
	"punpcklbw %%mm2, %%mm6\n"
	"movq      %%mm1, %%mm7\n"
	"punpcklbw %%mm3, %%mm7\n"
	"movq      %%mm0, %%mm4\n"
	"punpcklwd %%mm7, %%mm6\n"
	"movq      %%mm1, %%mm5\n"
	"movq      %%mm6, (%0)\n"
	"movq      %%mm0, %%mm6\n"
	"punpcklbw %%mm2, %%mm6\n"
	"punpckhwd %%mm7, %%mm6\n"
	"movq      %%mm6, 8(%0)\n"
	"punpckhbw %%mm2, %%mm4\n"
	"punpckhbw %%mm3, %%mm5\n"
	"punpcklwd %%mm5, %%mm4\n"
	"movq      %%mm4, 16(%0)\n"
	"movq      %%mm0, %%mm4\n"
	"punpckhbw %%mm2, %%mm4\n"
	"punpckhwd %%mm5, %%mm4\n"
	"movq      %%mm4, 24(%0)\n"
	: /* no outputs */
	: "r" (image) );
}

static __inline__ CARD32
loadyv12 (CARD8 *py,
	  CARD8 *pu,
	  CARD8 *pv)
{
    INT16 y, u, v;
    INT32 r, g, b;

    y = *py - 16;
    u = *pu - 128;
    v = *pv - 128;

    /* R = 1.164(Y - 16) + 1.596(V - 128) */
    r = 0x012b27 * y + 0x019a2e * v;
    /* G = 1.164(Y - 16) - 0.813(V - 128) - 0.391(U - 128) */
    g = 0x012b27 * y - 0x00d0f2 * v - 0x00647e * u;
    /* B = 1.164(Y - 16) + 2.018(U - 128) */
    b = 0x012b27 * y + 0x0206a2 * u;

    return 0xff000000 |
	(r >= 0 ? r < 0x1000000 ? r         & 0xff0000 : 0xff0000 : 0) |
	(g >= 0 ? g < 0x1000000 ? (g >> 8)  & 0x00ff00 : 0x00ff00 : 0) |
	(b >= 0 ? b < 0x1000000 ? (b >> 16) & 0x0000ff : 0x0000ff : 0);
}

typedef struct _ScanlineBuf {
    Bool   lock[2];
    int    y[2];
    CARD8 *line[2];
    int   height;
    CARD8 *heap;
} ScanlineBuf;

static Bool
init_scanline_buffer (ScanlineBuf *slb,
		      CARD8	  *buffer,
		      int	  size,
		      int	  length,
		      int	  height)
{
    int i, s;

    s = length << 1;

    if (size < s)
    {
	slb->heap = xalloc (s);
	if (!slb->heap)
	    return FALSE;

	buffer = slb->heap;
    }
    else
    {
	slb->heap = NULL;
    }

    for (i = 0; i < 2; i++)
    {
	slb->lock[i] = FALSE;
	slb->y[i]    = SHRT_MAX;
	slb->line[i] = buffer;

	buffer += length;
    }

    slb->height = height;

    return TRUE;
}

static void
fini_scanline_buffer (ScanlineBuf *slb)
{
    if (slb->heap)
	xfree (slb->heap);
}

static __inline__ void
release_scanlines (ScanlineBuf *slb)
{
    int i;

    for (i = 0; i < 2; i++)
	slb->lock[i] = FALSE;
}

static __inline__ int
_y_to_scanline (ScanlineBuf *slb,
		int	    y)
{
    return (y < 0) ? 0 : (y >= slb->height) ? slb->height - 1 : y;
}

static __inline__ CARD8 *
get_scanline (ScanlineBuf *slb,
	      int	  y)
{
    int i;

    y = _y_to_scanline (slb, y);

    for (i = 0; i < 2; i++)
    {
	if (slb->y[i] == y)
	{
	    slb->lock[i] = TRUE;
	    return slb->line[i];
	}
    }

    return NULL;
}

static __inline__ CARD8 *
loadyv12_scanline (ScanlineBuf *slb,
		   int	       y,
		   CARD8       *srcY,
		   int	       yStride,
		   CARD8       *srcU,
		   CARD8       *srcV,
		   int	       uvStride,
		   int	       x,
		   int	       width)
{
    CARD8 *py, *pu, *pv, *pd;
    int   i, w;

    y = _y_to_scanline (slb, y);

    for (i = 0; slb->lock[i]; i++);

    slb->y[i]    = y;
    slb->lock[i] = TRUE;

    py = srcY + yStride  * (y >> 0);
    pu = srcU + uvStride * (y >> 1);
    pv = srcV + uvStride * (y >> 1);

    pd = slb->line[i];

    w = width;

    while (w && (unsigned long) py & 7)
    {
	*((CARD32 *) pd) = loadyv12 (py, pu, pv);

	pd += 4;
	py += 1;

	if (w & 1)
	{
	    pu += 1;
	    pv += 1;
	}

	w--;
    }

    while (w >= 8)
    {
	mmx_loadyv12 (py, pu, pv);
	mmx_pack8888 (pd);

	py += 8;
	pu += 4;
	pv += 4;
	pd += 32;

	w -= 8;
    }

    while (w)
    {
	*((CARD32 *) pd) = loadyv12 (py, pu, pv);

	pd += 4;
	py += 1;

	if (w & 1)
	{
	    pu += 1;
	    pv += 1;
	}

	w--;
    }

    return slb->line[i];
}

static __inline__ CARD8
interpolate_bilinear (int   distx,
		      int   idistx,
		      int   disty,
		      int   idisty,
		      CARD8 tl,
		      CARD8 tr,
		      CARD8 bl,
		      CARD8 br)
{
    return ((tl * idistx + tr * distx) * idisty +
	    (bl * idistx + br * distx) * disty) >> 16;
}

static __inline__ void
interpolate_bilinear_8888 (int   distx,
			   int   idistx,
			   int   disty,
			   int   idisty,
			   CARD8 *l0,
			   CARD8 *l1,
			   int   x,
			   CARD8 buffer[4])
{
    buffer[0] = interpolate_bilinear (distx, idistx, disty, idisty,
				      l0[x], l0[x + 4],
				      l1[x], l1[x + 4]);

    buffer[1] = interpolate_bilinear (distx, idistx, disty, idisty,
				      l0[x + 1], l0[x + 5],
				      l1[x + 1], l1[x + 5]);

    buffer[2] = interpolate_bilinear (distx, idistx, disty, idisty,
				      l0[x + 2], l0[x + 6],
				      l1[x + 2], l1[x + 6]);

    buffer[3] = interpolate_bilinear (distx, idistx, disty, idisty,
				      l0[x + 3], l0[x + 7],
				      l1[x + 3], l1[x + 7]);
}

/* TODO: MMX code for bilinear interpolation */
void
fbCompositeSrc_yv12x8888mmx (CARD8      op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    PictTransform *transform = pSrc->transform;
    CARD8	  *dst, *srcY, *srcU, *srcV;
    FbBits	  *srcBits;
    FbStride	  srcStride, uvStride;
    int		  srcXoff;
    int		  srcYoff;
    FbBits	  *dstBits;
    FbStride	  dstStride;
    int		  dstXoff;
    int		  dstYoff;
    int		  bpp, offset, w;
    CARD8	  *pd;

    fbGetDrawable (pSrc->pDrawable, srcBits, srcStride, bpp, srcXoff, srcYoff);
    fbGetDrawable (pDst->pDrawable, dstBits, dstStride, bpp, dstXoff, dstYoff);

    dst = (CARD8 *) dstBits;
    dstStride *= sizeof (FbBits);

    srcY = (CARD8 *) srcBits;
    if (srcStride < 0)
    {
	offset = ((-srcStride) >> 1) * ((pSrc->pDrawable->height - 1) >> 1) -
	    srcStride;
	srcV = (CARD8 *) (srcBits + offset);
	offset += ((-srcStride) >> 1) * ((pSrc->pDrawable->height) >> 1);
	srcU = (CARD8 *) (srcBits + offset);
    }
    else
    {
	offset = srcStride * pSrc->pDrawable->height;

	srcV = (CARD8 *) (srcBits + offset);
	srcU = (CARD8 *) (srcBits + offset + (offset >> 2));
    }

    srcStride *= sizeof (FbBits);
    uvStride = srcStride >> 1;

    if (transform)
    {
	/* transformation is a Y coordinate flip, this is achieved by
	   moving start offsets for each plane and changing sign of stride */
	if (pSrc->transform->matrix[0][0] == (1 << 16)  &&
	    pSrc->transform->matrix[1][1] == -(1 << 16) &&
	    pSrc->transform->matrix[0][2] == 0          &&
	    pSrc->transform->matrix[1][2] == (pSrc->pDrawable->height << 16))
	{
	    srcY = srcY + ((pSrc->pDrawable->height >> 0) - 1) * srcStride;
	    srcU = srcU + ((pSrc->pDrawable->height >> 1) - 0) * uvStride;
	    srcV = srcV + ((pSrc->pDrawable->height >> 1) - 0) * uvStride;

	    srcStride = -srcStride;
	    uvStride  = -uvStride;

	    transform = 0;
	}
    }

    dst += dstStride * (yDst + dstYoff) + ((xDst + dstXoff) << 2);

    if (transform)
    {
	ScanlineBuf slb;
	CARD8	    _scanline_buf[8192];
	CARD8	    *ps, *ps0, *ps1;
	int	    x, x0, y, line, xStep, yStep;
	int         distx, idistx, disty, idisty;
	int	    srcEnd = (pSrc->pDrawable->width - 1) << 16;

	x0 = pSrc->transform->matrix[0][2] + ((xSrc + srcXoff) << 16);
	y  = pSrc->transform->matrix[1][2] + ((ySrc + srcYoff) << 16);

	xStep = pSrc->transform->matrix[0][0];
	yStep = pSrc->transform->matrix[1][1];

	init_scanline_buffer (&slb,
			      _scanline_buf, sizeof (_scanline_buf),
			      pSrc->pDrawable->width << 2,
			      pSrc->pDrawable->height);

	while (height--)
	{
	    disty  = (y >> 8) & 0xff;
	    idisty = 256 - disty;
	    line   = y >> 16;

	    ps0 = get_scanline (&slb, line);
	    ps1 = get_scanline (&slb, line + 1);

	    if (!ps0)
		ps0 = loadyv12_scanline (&slb, line,
					 srcY, srcStride, srcU, srcV, uvStride,
					 0, pSrc->pDrawable->width);

	    if (!ps1)
		ps1 = loadyv12_scanline (&slb, line + 1,
					 srcY, srcStride, srcU, srcV, uvStride,
					 0, pSrc->pDrawable->width);

	    pd = dst;

	    x = x0;
	    w = width;

	    if (pSrc->filter == PictFilterBilinear)
	    {
		while (w && x < 0)
		{
		    interpolate_bilinear_8888 (0, 256, disty, idisty,
					       ps0, ps1, 0, pd);

		    x  += xStep;
		    pd += 4;
		    w  -= 1;
		}

		while (w && x < srcEnd)
		{
		    distx  = (x >> 8) & 0xff;
		    idistx = 256 - distx;

		    interpolate_bilinear_8888 (distx, idistx, disty, idisty,
					       ps0, ps1, (x >> 14) & ~3, pd);

		    x  += xStep;
		    pd += 4;
		    w  -= 1;
		}

		while (w)
		{
		    interpolate_bilinear_8888 (256, 0, disty, idisty,
					       ps0, ps1, (x >> 14) & ~3, pd);

		    pd += 4;
		    w  -= 1;
		}
	    }
	    else
	    {
		while (w && x < 0)
		{
		    *(CARD32 *) pd = *(CARD32 *) ps0;

		    x  += xStep;
		    pd += 4;
		    w  -= 1;
		}

		while (w && x < srcEnd)
		{
		    *(CARD32 *) pd = ((CARD32 *) ps0)[x >> 16];

		    x  += xStep;
		    pd += 4;
		    w  -= 1;
		}

		while (w)
		{
		    *(CARD32 *) pd = ((CARD32 *) ps0)[x >> 16];

		    pd += 4;
		    w  -= 1;
		}
	    }

	    y   += yStep;
	    dst += dstStride;

	    release_scanlines (&slb);
	}

	fini_scanline_buffer (&slb);
    }
    else
    {
	CARD8 *py, *pu, *pv;

	srcY += srcStride * (ySrc >> 0) + srcYoff + ((xSrc + srcXoff) >> 0);
	srcU += uvStride  * (ySrc >> 1) + srcYoff + ((xSrc + srcXoff) >> 1);
	srcV += uvStride  * (ySrc >> 1) + srcYoff + ((xSrc + srcXoff) >> 1);

	while (height)
	{
	    py = srcY;
	    pu = srcU;
	    pv = srcV;
	    pd = dst;

	    w = width;

	    while (w && (unsigned long) py & 7)
	    {
		*((CARD32 *) pd) = loadyv12 (py, pu, pv);

		pd += 4;
		py += 1;

		if (w & 1)
		{
		    pu += 1;
		    pv += 1;
		}

		w--;
	    }

	    while (w >= 8)
	    {
		mmx_loadyv12 (py, pu, pv);
		mmx_pack8888 (pd);

		py += 8;
		pu += 4;
		pv += 4;
		pd += 32;

		w -= 8;
	    }

	    while (w)
	    {
		*((CARD32 *) pd) = loadyv12 (py, pu, pv);

		pd += 4;
		py += 1;

		if (w & 1)
		{
		    pu += 1;
		    pv += 1;
		}

		w--;
	    }

	    dst  += dstStride;
	    srcY += srcStride;

	    if (height & 1)
	    {
		srcU += uvStride;
		srcV += uvStride;
	    }

	    height--;
	}
    }

    _mm_empty ();
}

#endif /* RENDER */
#endif /* USE_MMX */
