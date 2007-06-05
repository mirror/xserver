#undef VERBOSE

/*****************************************************************

Copyright 2007 Sun Microsystems, Inc.

All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, and/or sell copies of the Software, and to permit persons
to whom the Software is furnished to do so, provided that the above
copyright notice(s) and this permission notice appear in all copies of
the Software and that both the above copyright notice(s) and this
permission notice appear in supporting documentation.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

Except as contained in this notice, the name of a copyright holder
shall not be used in advertising or otherwise to promote the sale, use
or other dealings in this Software without prior written authorization
of the copyright holder.

******************************************************************/

/* TODO: prune these */
#include <unistd.h>
#include "regionstr.h"
#include "damage.h"
#include "windowstr.h"
#include "remwin.h"
#include "rwcomm.h"
#include "misc.h"
#include "protocol.h"
#include "cursorstr.h"
#include "servermd.h"
#include "rwcommsock.h"

extern Bool rwRleVerifyPixels;

#define RGB_MASK 0x00ffffff

#define ROUNDUP(fval) (int)((fval)+1)

#define MIN(a, b) (((a)>(b))?(b):(a))

#define MAX_COUNT 255

#define NEXT_PIXEL(pixel) { \
    if (linesLeftInChunk <= 0) { \
	/* End of chunk */ \
	(pixel) = -1; \
        /*ErrorF("y,x,ll at end of chunk = %d, %d, %d\n", y, x, linesLeftInChunk);*/ \
    } else {\
        (pixel) = *(pPixel++) & RGB_MASK; \
        /* Advance pointers */ \
        if (++x >= xLimit) { \
	    y++; \
	    linesLeftInChunk--; \
            pLine += w; \
	    pPixel = pLine; \
	    x = xStart;\
        } \
    } \
}

#ifdef VERBOSE

static int maxVerboseRuns = 120;
static int numRunsOutput = 0;

#define OUTPUT_PIXEL(count, pixel) { \
    /* \
    if (numRunsOutput++ < maxVerboseRuns) { \
        ErrorF("numRuns = %d, count = %d, pixel = 0x%x\n", numRunsOutput, count, pixel); \
    } \
    */ \
    if (pDst >= pDstLimit) { \
	FatalError("Buffer overflow, pDst = 0x%x, 0x%x\n", (int)pDst, (int)pDstLimit); \
    } \
    *pDst++ = ((count) << 24) | (pixel); \
    bytesInChunk += 4; \
}
#else
#define OUTPUT_PIXEL(count, pixel) { \
    *pDst++ = ((count) << 24) | (pixel); \
    bytesInChunk += 4; \
}
#endif /* VERBOSE */

Bool
rlePixelsWrite (ScreenPtr pScreen, WindowPtr pWin, int x, int y, int w, int h)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[DIRTY_AREA_RECT_SIZE];
    DirtyAreaRectPtr pDirty = (DirtyAreaRectPtr) &buf[0];
    int *pSrc, *pDst, *pLine, *pPixel, *pChunkHeight, *pNumOutBytes;
    int linesLeft, xStart, xLimit, chunkHeight;
    int numChunks, linesLeftInChunk;
    int count, a, b;
    int bytesInChunk;
    int i, n;

    /* TODO: for debug */
    int *pDstLimit;

    chunkHeight = RLE_BUF_MAX_NUM_PIXELS / w;
    numChunks = ROUNDUP((float)h / chunkHeight);
#ifdef VERBOSE
    ErrorF("chunkHeight = %d\n", chunkHeight);
    ErrorF("numChunks = %d\n", numChunks);
#endif /* VERBOSE */
    
    /* Prepare rectangle description */
    pDirty->x = x;
    pDirty->y = y;
    pDirty->width = w;
    /* TODO: HACK: notify the client of verification via negative numChunks */
    if (rwRleVerifyPixels) {
	pDirty->height = -numChunks;
    } else {
	pDirty->height = numChunks;
    }
    pDirty->encodingType = DISPLAY_PIXELS_ENCODING_RLE24;
#ifdef VERBOSE
    ErrorF("x = %d\n", x);
    ErrorF("y = %d\n", y);
    ErrorF("y = %d\n", h);
    ErrorF("w = %d\n", w);
#endif /* VERBOSE */

    swaps(&pDirty->x, n);
    swaps(&pDirty->y, n);
    swaps(&pDirty->width, n);
    swaps(&pDirty->height, n);
    swapl(&pDirty->encodingType, a);

    /* Send it off */
    if (!RWCOMM_BUFFER_WRITE(pComm, buf, DIRTY_AREA_RECT_SIZE)) {    
	return FALSE;
    }

    /* Lazy allocation of pixel buffer*/
    if (pScrPriv->pPixelBuf == NULL) {
	pScrPriv->pPixelBuf = (unsigned char *) 
	    xalloc(PIXEL_BUF_MAX_NUM_BYTES);
    }
    pSrc = (int *) pScrPriv->pPixelBuf;

    /* Lazy allocation of rle output buffer*/
    if (pScrPriv->pRleBuf == NULL) {
	pScrPriv->pRleBuf = (unsigned char *) 
	    xalloc(RLE_BUF_MAX_NUM_BYTES + 10);
    }

    linesLeft = h;
    xStart = x;
    xLimit = x + w;

    pChunkHeight = (int *) buf;
    for (i = 0; i < numChunks; i++) {
	
	/* ErrorF("Filling chunk %d\n", i); */

	/* This is a fresh chunk; start at the beginning of the buffer */
	pDst = (int *) pScrPriv->pRleBuf;
	pDstLimit = (int *) ((char *)pDst + RLE_BUF_MAX_NUM_BYTES + 10);

	/* Clamp chunk height and send it */
	chunkHeight = MIN(linesLeft, chunkHeight);
	*pChunkHeight = chunkHeight;
	swapl(pChunkHeight, n);
	if (!RWCOMM_BUFFER_WRITE(pComm, buf, 4)) { 
	    return FALSE;
	}

	/* Fetch chunk from frame buffer */
	(*pScreen->GetImage)((DrawablePtr)pWin, x, y, w, chunkHeight, 
			     ZPixmap, ~0, (char *) pSrc);
	pLine = pSrc;
	pPixel = pLine;
    
	linesLeftInChunk = chunkHeight;
	bytesInChunk = 0;
	count = 1;
	NEXT_PIXEL(a);
	NEXT_PIXEL(b);

	while (b >= 0) {
	    if (a == b) {
		if (count == MAX_COUNT) {
		    OUTPUT_PIXEL(count, a);
		    count = 1;
		} else {
		    count++;
		}	
	    } else {
		OUTPUT_PIXEL(count, a);
		a = b;
		count = 1;
	    }
	    NEXT_PIXEL(b);
	}
	OUTPUT_PIXEL(count, a);
		    
	/* 
	** Now send the number of bytes in the chunk followed by 
	** the chunk data.
	*/
	pNumOutBytes = (int *) buf;
	*pNumOutBytes = bytesInChunk;
	swapl(pNumOutBytes, n);
	if (!RWCOMM_BUFFER_WRITE(pComm, buf, 4)) { 
	    return FALSE;
	}
	if (!RWCOMM_BUFFER_WRITE(pComm, (char *)pScrPriv->pRleBuf, bytesInChunk)) { 
	    return FALSE;
	}

	linesLeft -= chunkHeight;
    }

    return TRUE;
}
