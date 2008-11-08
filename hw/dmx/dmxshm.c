/*
 * Copyright © 2008 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "dmx.h"
#include "dmxshm.h"
#include "dmxgc.h"
#include "dmxwindow.h"
#include "dmxpixmap.h"
#include "dmxsync.h"
#include "dmxlog.h"
#include "scrnintstr.h"
#include "servermd.h"
#include "shmint.h"

#ifdef PANORAMIX
#include "panoramiX.h"
#include "panoramiXsrv.h"
#endif

#ifdef MITSHM

#include <sys/ipc.h>
#include <sys/shm.h>

unsigned long DMX_SHMSEG;

static dmxShmSegInfoPtr shmSegs = NULL;

extern int (*ProcShmVector[ShmNumberRequests])(ClientPtr);

static int (*dmxSaveProcVector[ShmNumberRequests]) (ClientPtr);

static void dmxShmPutImage (XSHM_PUT_IMAGE_ARGS);

static ShmFuncs dmxFuncs = { NULL, dmxShmPutImage };

void
ShmRegisterDmxFuncs (ScreenPtr pScreen)
{
    ShmRegisterFuncs (pScreen, &dmxFuncs);
}

static void
dmxShmPutImage (DrawablePtr  pDraw,
		GCPtr	     pGC,
		int	     depth,
		unsigned int format,
		int	     w,
		int	     h,
		int	     sx,
		int	     sy,
		int	     sw,
		int	     sh,
		int	     dx,
		int	     dy,
		char	     *data)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDraw->pScreen->myNum];
    dmxGCPrivPtr  pGCPriv = DMX_GET_GC_PRIV (pGC);
    int           vIndex = dmxScreen->beDefVisualIndex;
    XImage        *image = NULL;
    Drawable      draw;

    if (pDraw->type == DRAWABLE_WINDOW)
	draw = (DMX_GET_WINDOW_PRIV ((WindowPtr) (pDraw)))->window;
    else
	draw = (DMX_GET_PIXMAP_PRIV ((PixmapPtr) (pDraw)))->pixmap;

    if (!dmxScreen->beDisplay || !draw)
	return;

    XLIB_PROLOGUE (dmxScreen);
    image = XCreateImage (dmxScreen->beDisplay,
			  dmxScreen->beVisuals[vIndex].visual,
			  depth, format, 0, data, w, h,
			  BitmapPad (dmxScreen->beDisplay),
			  (format == ZPixmap) ?
			  PixmapBytePad (w, depth) : BitmapBytePad (w));
    XLIB_EPILOGUE (dmxScreen);

    if (image)
    {
	RegionRec reg;
	BoxRec    src, dst;
	BoxPtr    pBox;
	int       nBox;

	src.x1 = dx - sx;
	src.y1 = dy - sy;
	src.x2 = src.x1 + w;
	src.y2 = src.y1 + h;

	dst.x1 = dx;
	dst.y1 = dy;
	dst.x2 = dst.x1 + sw;
	dst.y2 = dst.y1 + sh;

	if (src.x1 > dst.x1)
	    dst.x1 = src.x1;
	if (src.y1 > dst.y1)
	    dst.y1 = src.y1;
	if (src.x2 < dst.x2)
	    dst.x2 = src.x2;
	if (src.y2 < dst.y2)
	    dst.y2 = src.y2;

	REGION_INIT (pGC->pScreen, &reg, &dst, 0);

	if (pGC->pCompositeClip)
	{
	    REGION_TRANSLATE (pGC->pScreen, pGC->pCompositeClip,
			      -pDraw->x, -pDraw->y);
	    REGION_INTERSECT (pGC->pScreen, &reg, &reg, pGC->pCompositeClip);
	    REGION_TRANSLATE (pGC->pScreen, pGC->pCompositeClip,
			      pDraw->x, pDraw->y);
	}

	nBox = REGION_NUM_RECTS (&reg);
	pBox = REGION_RECTS (&reg);

	XLIB_PROLOGUE (dmxScreen);
	while (nBox--)
	{
	    XPutImage (dmxScreen->beDisplay, draw, pGCPriv->gc, image,
		       pBox->x1 - src.x1,
		       pBox->y1 - src.y1,
		       pBox->x1,
		       pBox->y1,
		       pBox->x2 - pBox->x1,
		       pBox->y2 - pBox->y1);
	    pBox++;
	}
	XLIB_EPILOGUE (dmxScreen);

	REGION_UNINIT (pGC->pScreen, &reg);

	XFree (image);

	dmxSync (dmxScreen, FALSE);
    }
}

void
dmxBEAttachShmSeg (DMXScreenInfo    *dmxScreen,
		   dmxShmSegInfoPtr pShmInfo)
{
    if (!dmxScreen->beShm)
	return;

    if (!pShmInfo->shmseg[dmxScreen->index])
    {
	pShmInfo->shmseg[dmxScreen->index] =
	    xcb_generate_id (dmxScreen->connection);

	xcb_shm_attach (dmxScreen->connection,
			pShmInfo->shmseg[dmxScreen->index],
			pShmInfo->shmid,
			pShmInfo->readOnly);
    }
}

void
dmxBEDetachShmSeg (DMXScreenInfo    *dmxScreen,
		   dmxShmSegInfoPtr pShmInfo)
{
    if (!dmxScreen->beShm)
	return;

    if (pShmInfo->shmseg[dmxScreen->index])
    {
	xcb_shm_detach (dmxScreen->connection,
			pShmInfo->shmseg[dmxScreen->index]);

	pShmInfo->shmseg[dmxScreen->index] = None;
	pShmInfo->cookie[dmxScreen->index].sequence = 0;
    }
}

Bool
dmxScreenEventCheckShm (ScreenPtr           pScreen,
			xcb_generic_event_t *event)
{
    DMXScreenInfo    *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxShmSegInfoPtr pShmInfo;
    xcb_shm_seg_t    shmseg = 0;
    unsigned int     sequence = 0;

    if (!dmxScreen->beShm)
	return FALSE;

    if (event->response_type)
    {
	switch ((event->response_type & ~0x80) - dmxScreen->beShmEventBase) {
	case XCB_SHM_COMPLETION: {
	    /* XCB protocol description for XCB_SHM_COMPLETION is wrong */
	    xShmCompletionEvent *xcompletion =
		(xShmCompletionEvent *) event;

	    shmseg = xcompletion->shmseg;
	} break;
	default:
	    return FALSE;
	}
    }
    else
    {
	sequence = ((xcb_generic_error_t *) event)->sequence;
    }

    for (pShmInfo = shmSegs; pShmInfo; pShmInfo = pShmInfo->next)
    {
	if (shmseg && shmseg != pShmInfo->shmseg[pScreen->myNum])
	    continue;

	if (!pShmInfo->cookie[pScreen->myNum].sequence)
	    continue;

	if (sequence && sequence != pShmInfo->cookie[pScreen->myNum].sequence)
	    continue;
	    
	pShmInfo->pendingEvents--;
	pShmInfo->cookie[pScreen->myNum].sequence = 0;
    }

    return TRUE;
}

static int
dmxFreeShmSeg (pointer value,
	       XID     id)
{
    dmxShmSegInfoPtr *pPrev, pShmInfo = (dmxShmSegInfoPtr) value;
    int		     i;

    if (--pShmInfo->refcnt)
	return TRUE;

    for (i = 0; i < dmxNumScreens; i++)
    {
	DMXScreenInfo *dmxScreen = &dmxScreens[i];

	if (!dmxScreen->beDisplay)
	    continue;

	dmxBEDetachShmSeg (dmxScreen, pShmInfo);
    }

    for (pPrev = &shmSegs; *pPrev != pShmInfo; pPrev = &(*pPrev)->next)
	;
    *pPrev = pShmInfo->next;

    xfree (pShmInfo);
    return Success;
}

static int
dmxProcShmAttach (ClientPtr client)
{
    dmxShmSegInfoPtr pShmInfo;
    int		     i, err;
    REQUEST(xShmAttachReq);

    err = (*dmxSaveProcVector[X_ShmAttach]) (client);
    if (err != Success)
	return err;

    for (pShmInfo = shmSegs;
	 pShmInfo && (pShmInfo->shmid != stuff->shmid);
	 pShmInfo = pShmInfo->next)
	;
    if (pShmInfo)
    {
	pShmInfo->refcnt++;
    }
    else
    {
	pShmInfo = xalloc (sizeof (dmxShmSegInfoRec));
	if (!pShmInfo)
	    return Success;

	pShmInfo->refcnt        = 1;
	pShmInfo->shmid         = stuff->shmid;
	pShmInfo->readOnly      = stuff->readOnly;
	pShmInfo->pendingEvents = 0;

	memset (pShmInfo->shmseg, 0, sizeof (pShmInfo->shmseg));
	memset (pShmInfo->cookie, 0, sizeof (pShmInfo->cookie));

	pShmInfo->next = shmSegs;
	shmSegs = pShmInfo;

	for (i = 0; i < dmxNumScreens; i++)
	{
	    DMXScreenInfo *dmxScreen = &dmxScreens[i];

	    if (!dmxScreen->beDisplay)
		continue;

	    dmxBEAttachShmSeg (dmxScreen, pShmInfo);
	}
    }

    AddResource (stuff->shmseg, DMX_SHMSEG, (pointer) pShmInfo);

    return Success;
}

static int
dmxProcShmGetImage (ClientPtr client)
{
    DrawablePtr		pDraw;
    long		lenPer = 0, length;
    Mask		plane = 0;
    xShmGetImageReply	xgi;
    ShmDescPtr		shmdesc;
    dmxShmSegInfoPtr    pShmInfo;
    Drawable            draw;
    int			n, rc;

    REQUEST(xShmGetImageReq);

    REQUEST_SIZE_MATCH(xShmGetImageReq);
    if ((stuff->format != XYPixmap) && (stuff->format != ZPixmap))
    {
	client->errorValue = stuff->format;
        return(BadValue);
    }
    rc = dixLookupDrawable(&pDraw, stuff->drawable, client, 0,
			   DixReadAccess);
    if (rc != Success)
	return rc;

    VERIFY_SHMPTR(stuff->shmseg, stuff->offset, TRUE, shmdesc, client);

    for (pShmInfo = shmSegs;
	 pShmInfo && (pShmInfo->shmid != shmdesc->shmid);
	 pShmInfo = pShmInfo->next)
	;
	
    if (!pShmInfo || !pShmInfo->shmseg[pDraw->pScreen->myNum])
	return (*dmxSaveProcVector[X_ShmGetImage]) (client);

    if (pDraw->type == DRAWABLE_WINDOW)
    {
      if( /* check for being viewable */
	 !((WindowPtr) pDraw)->realized ||
	  /* check for being on screen */
         pDraw->x + stuff->x < 0 ||
 	 pDraw->x + stuff->x + (int)stuff->width > pDraw->pScreen->width ||
         pDraw->y + stuff->y < 0 ||
         pDraw->y + stuff->y + (int)stuff->height > pDraw->pScreen->height ||
          /* check for being inside of border */
         stuff->x < - wBorderWidth((WindowPtr)pDraw) ||
         stuff->x + (int)stuff->width >
		wBorderWidth((WindowPtr)pDraw) + (int)pDraw->width ||
         stuff->y < -wBorderWidth((WindowPtr)pDraw) ||
         stuff->y + (int)stuff->height >
		wBorderWidth((WindowPtr)pDraw) + (int)pDraw->height
        )
	    return(BadMatch);
	xgi.visual = wVisual(((WindowPtr)pDraw));
	draw = (DMX_GET_WINDOW_PRIV ((WindowPtr) (pDraw)))->window;
    }
    else
    {
	if (stuff->x < 0 ||
	    stuff->x+(int)stuff->width > pDraw->width ||
	    stuff->y < 0 ||
	    stuff->y+(int)stuff->height > pDraw->height
	    )
	    return(BadMatch);
	xgi.visual = None;
	draw = (DMX_GET_PIXMAP_PRIV ((PixmapPtr) (pDraw)))->pixmap;
    }
    xgi.type = X_Reply;
    xgi.length = 0;
    xgi.sequenceNumber = client->sequence;
    xgi.depth = pDraw->depth;
    if(stuff->format == ZPixmap)
    {
	length = PixmapBytePad(stuff->width, pDraw->depth) * stuff->height;
    }
    else 
    {
	lenPer = PixmapBytePad(stuff->width, 1) * stuff->height;
	plane = ((Mask)1) << (pDraw->depth - 1);
	/* only planes asked for */
	length = lenPer * Ones(stuff->planeMask & (plane | (plane - 1)));
    }

    if (!draw)
	return (*dmxSaveProcVector[X_ShmGetImage]) (client);

    VERIFY_SHMSIZE(shmdesc, stuff->offset, length, client);
    xgi.size = length;

    if (length)
    {
	xcb_shm_get_image_cookie_t cookie;
	xcb_shm_get_image_reply_t  *reply;
	DMXScreenInfo		   *dmxScreen =
	    &dmxScreens[pDraw->pScreen->myNum];

	rc = BadValue;
	cookie = xcb_shm_get_image (dmxScreen->connection, draw,
				    stuff->x, stuff->y,
				    stuff->width, stuff->height,
				    stuff->planeMask, stuff->format,
				    pShmInfo->shmseg[dmxScreen->index],
				    stuff->offset);
	do {
	    dmxDispatch ();

	    if (xcb_poll_for_reply (dmxScreen->connection,
				    cookie.sequence,
				    (void **) &reply,
				    NULL))
	    {
		if (reply)
		{
		    rc = Success;
		    free (reply);
		}
		break;
	    }
	} while (dmxWaitForResponse () && dmxScreen->alive);

	if (rc != Success)
	    return (*dmxSaveProcVector[X_ShmGetImage]) (client);
    }

    if (client->swapped) {
    	swaps(&xgi.sequenceNumber, n);
    	swapl(&xgi.length, n);
	swapl(&xgi.visual, n);
	swapl(&xgi.size, n);
    }
    WriteToClient(client, sizeof(xShmGetImageReply), (char *)&xgi);

    return(client->noClientException);
}

static int
dmxProcShmPutImage (ClientPtr client)
{
    GCPtr pGC;
    DrawablePtr pDraw;
    long length;
    ShmDescPtr shmdesc;
    dmxShmSegInfoPtr pShmInfo;
    REQUEST(xShmPutImageReq);

    REQUEST_SIZE_MATCH(xShmPutImageReq);
    VALIDATE_DRAWABLE_AND_GC(stuff->drawable, pDraw, DixWriteAccess);
    VERIFY_SHMPTR(stuff->shmseg, stuff->offset, FALSE, shmdesc, client);
    if ((stuff->sendEvent != xTrue) && (stuff->sendEvent != xFalse))
	return BadValue;
    if (stuff->format == XYBitmap)
    {
        if (stuff->depth != 1)
            return BadMatch;
        length = PixmapBytePad(stuff->totalWidth, 1);
    }
    else if (stuff->format == XYPixmap)
    {
        if (pDraw->depth != stuff->depth)
            return BadMatch;
        length = PixmapBytePad(stuff->totalWidth, 1);
	length *= stuff->depth;
    }
    else if (stuff->format == ZPixmap)
    {
        if (pDraw->depth != stuff->depth)
            return BadMatch;
        length = PixmapBytePad(stuff->totalWidth, stuff->depth);
    }
    else
    {
	client->errorValue = stuff->format;
        return BadValue;
    }

    /* 
     * There's a potential integer overflow in this check:
     * VERIFY_SHMSIZE(shmdesc, stuff->offset, length * stuff->totalHeight,
     *                client);
     * the version below ought to avoid it
     */
    if (stuff->totalHeight != 0 && 
	length > (shmdesc->size - stuff->offset)/stuff->totalHeight) {
	client->errorValue = stuff->totalWidth;
	return BadValue;
    }
    if (stuff->srcX > stuff->totalWidth)
    {
	client->errorValue = stuff->srcX;
	return BadValue;
    }
    if (stuff->srcY > stuff->totalHeight)
    {
	client->errorValue = stuff->srcY;
	return BadValue;
    }
    if ((stuff->srcX + stuff->srcWidth) > stuff->totalWidth)
    {
	client->errorValue = stuff->srcWidth;
	return BadValue;
    }
    if ((stuff->srcY + stuff->srcHeight) > stuff->totalHeight)
    {
	client->errorValue = stuff->srcHeight;
	return BadValue;
    }

    for (pShmInfo = shmSegs;
	 pShmInfo && (pShmInfo->shmid != shmdesc->shmid);
	 pShmInfo = pShmInfo->next)
	;
	
    if (pShmInfo && pShmInfo->shmseg[pDraw->pScreen->myNum])
    {
	DMXScreenInfo  *dmxScreen = &dmxScreens[pDraw->pScreen->myNum];
	dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV (pGC);
	xcb_drawable_t draw;

	if (pDraw->type == DRAWABLE_WINDOW)
	    draw = (DMX_GET_WINDOW_PRIV ((WindowPtr) (pDraw)))->window;
	else
	    draw = (DMX_GET_PIXMAP_PRIV ((PixmapPtr) (pDraw)))->pixmap;

	if (dmxScreen->beDisplay && draw)
	{
	    pShmInfo->cookie[dmxScreen->index] =
		xcb_shm_put_image (dmxScreen->connection,
				   draw,
				   XGContextFromGC (pGCPriv->gc),
				   stuff->totalWidth, stuff->totalHeight,
				   stuff->srcX, stuff->srcY,
				   stuff->srcWidth, stuff->srcHeight,
				   stuff->dstX, stuff->dstY,
				   stuff->depth,
				   stuff->format,
				   TRUE,
				   pShmInfo->shmseg[dmxScreen->index],
				   stuff->offset);

	    pShmInfo->pendingEvents++;
	}
    }
    else
    {
	dmxShmPutImage (pDraw, pGC, stuff->depth, stuff->format,
			stuff->totalWidth, stuff->totalHeight,
			stuff->srcX, stuff->srcY,
			stuff->srcWidth, stuff->srcHeight,
			stuff->dstX, stuff->dstY,
			shmdesc->addr + stuff->offset);
    }

    if (!pDraw->pScreen->myNum)
    {
	/* we could handle this completely asynchrounsly and continue to
	   process client requests until all pending completion events
	   have been collected but some clients seem to assume that
	   the server is done using the shared memory segment once it
	   has processed the request */
	if (pShmInfo)
	{
	    /* wait for all back-end servers to complete */
	    do {
		dmxDispatch ();
	    } while (pShmInfo->pendingEvents && dmxWaitForResponse ());
	}
    }

    if (stuff->sendEvent)
    {
	xShmCompletionEvent ev;

	ev.type = ShmCompletionCode;
	ev.drawable = stuff->drawable;
	ev.sequenceNumber = client->sequence;
	ev.minorEvent = X_ShmPutImage;
	ev.majorEvent = ShmReqCode;
	ev.shmseg = stuff->shmseg;
	ev.offset = stuff->offset;
	WriteEventsToClient(client, 1, (xEvent *) &ev);
    }

    return (client->noClientException);
}

void
dmxBEShmScreenInit (ScreenPtr pScreen)
{
    DMXScreenInfo             *dmxScreen = &dmxScreens[pScreen->myNum];
    xcb_generic_error_t       *error;
    xcb_shm_seg_t             shmseg;
    xcb_pixmap_t              pixmap = 0;
    uint32_t                  shmid;
    static char               key[] = { 'x', 'd', 'm', 'x' };
    char                      *shmaddr;
    xcb_shm_get_image_reply_t *reply;
    XlibGC                    gc;
    XGCValues                 gcvals;
    unsigned long             mask;
    int                       i;

    dmxScreen->beShm = FALSE;

    shmid = shmget (IPC_PRIVATE, sizeof (key), IPC_CREAT | 0600);
    if (shmid == -1)
	return;

    shmaddr = shmat (shmid, NULL, 0);
    if (shmaddr == (char *) -1)
    {
	shmctl (shmid, IPC_RMID, NULL);
	return;
    }

    memset (shmaddr, 0, sizeof (key));

    shmseg = xcb_generate_id (dmxScreen->connection);
    error  = xcb_request_check (dmxScreen->connection,
				xcb_shm_attach_checked (dmxScreen->connection,
							shmseg,
							shmid,
							FALSE));
    if (error)
    {
	free (error);
	shmdt (shmaddr);
	shmctl (shmid, IPC_RMID, NULL);
	return;
    }

    mask = (GCFunction | GCPlaneMask | GCClipMask | GCGraphicsExposures);

    gcvals.function           = GXcopy;
    gcvals.plane_mask         = AllPlanes;
    gcvals.clip_mask          = None;
    gcvals.foreground         = 0;
    gcvals.graphics_exposures = FALSE;

    pixmap = xcb_generate_id (dmxScreen->connection);
    xcb_create_pixmap (dmxScreen->connection,
		       8,
		       pixmap,
		       dmxScreen->scrnWin,
		       sizeof (key),
		       1);

    XLIB_PROLOGUE (dmxScreen);
    gc = XCreateGC (dmxScreen->beDisplay, pixmap, mask, &gcvals);
    if (gc)
    {
	for (i = 0; i < sizeof (key); i++)
	{
	    gcvals.foreground = key[i];
	    XChangeGC (dmxScreen->beDisplay, gc, GCForeground, &gcvals);
	    XFillRectangle (dmxScreen->beDisplay, pixmap, gc, i, 0, 1, 1);
	}

	XFreeGC (dmxScreen->beDisplay, gc);
    }
    XLIB_EPILOGUE (dmxScreen);

    reply =
	xcb_shm_get_image_reply (dmxScreen->connection,
				 xcb_shm_get_image (dmxScreen->connection,
						    pixmap,
						    0, 0,
						    sizeof (key), 1,
						    0xff,
						    XCB_IMAGE_FORMAT_Z_PIXMAP,
						    shmseg,
						    0),
				 NULL);

    xcb_free_pixmap (dmxScreen->connection, pixmap);
    
    if (!reply)
    {
	xcb_shm_detach (dmxScreen->connection, shmseg);
	shmdt (shmaddr);
	shmctl (shmid, IPC_RMID, NULL);
	return;
    }

    free (reply);

    for (i = 0; i < sizeof (key); i++)
	if (shmaddr[i] != key[i])
	    break;

    xcb_shm_detach (dmxScreen->connection, shmseg);
    shmdt (shmaddr);
    shmctl (shmid, IPC_RMID, NULL);

    if (i < sizeof (key))
	return;

    dmxScreen->beShm = TRUE;
    dmxScreen->beShmEventBase = XShmGetEventBase (dmxScreen->beDisplay);
    dmxLogOutput (dmxScreen, "Using MIT-SHM extension\n");
}

void dmxInitShm (void)
{
    int i;

    DMX_SHMSEG = CreateNewResourceType (dmxFreeShmSeg);

    for (i = 0; i < ShmNumberRequests; i++)
	dmxSaveProcVector[i] = ProcShmVector[i];

    ProcShmVector[X_ShmAttach]   = dmxProcShmAttach;
    ProcShmVector[X_ShmGetImage] = dmxProcShmGetImage;
    ProcShmVector[X_ShmPutImage] = dmxProcShmPutImage;
}

void dmxResetShm (void)
{
    int i;

    for (i = 0; i < ShmNumberRequests; i++)
	ProcShmVector[i] = dmxSaveProcVector[i];
}

#endif
