/* $Xorg: GCOps.h,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
/*

Copyright 1993 by Davor Matic

Permission to use, copy, modify, distribute, and sell this software
and its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation.  Davor Matic makes no representations about
the suitability of this software for any purpose.  It is provided "as
is" without express or implied warranty.

*/
/* $XFree86$ */

#ifndef XNESTGCOPS_H
#define XNESTGCOPS_H

void xsFillSpans(DrawablePtr pDrawable, GCPtr pGC, int nSpans,
		    xPoint *pPoints, int *pWidths, int fSorted);
void xsSetSpans(DrawablePtr pDrawable, GCPtr pGC, char *pSrc,
		   xPoint *pPoints, int *pWidths, int nSpans, int fSorted);
void xsGetSpans(DrawablePtr pDrawable, int maxWidth, DDXPointPtr pPoints,
		   int *pWidths, int nSpans, char *pBuffer);
void xsQueryBestSize(int class, unsigned short *pWidth,
			unsigned short *pHeight, ScreenPtr pScreen);
void xsPutImage(DrawablePtr pDrawable, GCPtr pGC, int depth, int x, int y,
		   int w, int h, int leftPad, int format, char *pImage);
void xsGetImage(DrawablePtr pDrawable, int x, int y, int w, int h,
		   unsigned int format, unsigned long planeMask,
		   char *pImage);
RegionPtr xsCopyArea(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
			GCPtr pGC, int srcx, int srcy, int width, int height,
			int dstx, int dsty);
RegionPtr xsCopyPlane(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
			 GCPtr pGC, int srcx, int srcy, int width, int height,
			 int dstx, int dsty, unsigned long plane);
void xsPolyPoint(DrawablePtr pDrawable, GCPtr pGC, int mode, int nPoints,
		    DDXPointPtr  pPoints);
void xsPolylines(DrawablePtr pDrawable, GCPtr pGC, int mode, int nPoints,
		    DDXPointPtr pPoints);
void xsPolySegment(DrawablePtr pDrawable, GCPtr pGC, int nSegments,
		      xSegment *pSegments);
void xsPolyRectangle(DrawablePtr pDrawable, GCPtr pGC, int nRectangles,
			xRectangle *pRectangles);
void xsPolyArc(DrawablePtr pDrawable, GCPtr pGC, int nArcs, xArc *pArcs);
void xsFillPolygon(DrawablePtr pDrawable, GCPtr pGC, int shape, int mode,
		      int nPoints, DDXPointPtr pPoints);
void xsPolyFillRect(DrawablePtr pDrawable, GCPtr pGC, int nRectangles,
		       xRectangle *pRectangles);
void xsPolyFillArc(DrawablePtr pDrawable, GCPtr pGC, int nArcs, xArc *pArcs);
int xsPolyText8(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count,
		   char *string);
int xsPolyText16(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count,
		    unsigned short *string);
void xsImageText8(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count,
		    char *string);
void xsImageText16(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count,
		    unsigned short *string);
void xsImageGlyphBlt(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
			unsigned int nGlyphs, CharInfoPtr *pCharInfo,
			pointer pGlyphBase);
void xsPolyGlyphBlt(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
		       unsigned int nGlyphs, CharInfoPtr *pCharInfo,
		       pointer pGlyphBase);
void xsPushPixels(GCPtr pGC, PixmapPtr pBitmap, DrawablePtr pDrawable,
		     int width, int height, int x, int y);

#endif /* XNESTGCOPS_H */
