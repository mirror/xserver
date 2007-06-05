
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

#ifndef PROTOCOL_H
#define PROTOCOL_H

/*
** Client-to-Server Messages
*/

#define CLIENT_MESSAGE_TYPE_INVALID     	0
#define CLIENT_MESSAGE_TYPE_KEY			1
#define CLIENT_MESSAGE_TYPE_POINTER		2
#define CLIENT_MESSAGE_TYPE_TAKE_CONTROL	3
#define CLIENT_MESSAGE_TYPE_SET_WINDOW_TITLES	4
#define CLIENT_MESSAGE_TYPE_MOVE_WINDOW		5
#define CLIENT_MESSAGE_TYPE_RESIZE_WINDOW	6
#define CLIENT_MESSAGE_TYPE_DESTROY_WINDOW	7
#define CLIENT_MESSAGE_TYPE_HELLO	        8

typedef struct keyeventmessage_struct {
    CARD8 	msgType;			
    CARD8       isPressed;
    CARD16      pad;
    CARD32      keysym;
    CARD32      clientId;
} KeyEventMessage;

#define KEY_EVENT_MESSAGE_SIZE sizeof(KeyEventMessage)

#define KEY_EVENT_MESSAGE_GET_ISPRESSED(pBuf) \
    ((KeyEventMessage*)(pBuf))->isPressed

#define KEY_EVENT_MESSAGE_GET_KEYSYM(pBuf) \
    ((KeyEventMessage*)(pBuf))->keysym

#define KEY_EVENT_MESSAGE_GET_CLIENTID(pBuf) \
    ((KeyEventMessage*)(pBuf))->clientId

typedef struct pointereventmessage_struct {
    CARD8 	msgType;
    CARD8       mask;
    CARD16      x;
    CARD16      y;
    CARD16      pad;
    CARD32      wid;
    CARD32      clientId;
} PointerEventMessage;

#define POINTER_EVENT_MESSAGE_SIZE sizeof(PointerEventMessage)

#define POINTER_EVENT_MESSAGE_GET_MASK(pBuf) \
    ((PointerEventMessage *)(pBuf))->mask

#define POINTER_EVENT_MESSAGE_GET_X(pBuf) \
    ((PointerEventMessage*)(pBuf))->x

#define POINTER_EVENT_MESSAGE_GET_Y(pBuf) \
    ((PointerEventMessage*)(pBuf))->y

#define POINTER_EVENT_MESSAGE_GET_WID(pBuf) \
    ((PointerEventMessage*)(pBuf))->wid

#define POINTER_EVENT_MESSAGE_GET_CLIENTID(pBuf) \
    ((PointerEventMessage*)(pBuf))->clientId

typedef struct takecontrolmessage_struct {
    CARD8 	msgType;			
    CARD8       steal;
    CARD16      pad;
    CARD32      clientId;
} TakeControlMessage;

#define TAKE_CONTROL_MESSAGE_SIZE sizeof(TakeControlMessage)

#define TAKE_CONTROL_MESSAGE_GET_STEAL(pBuf) \
    ((((TakeControlMessage*)(pBuf))->steal == 1) ? TRUE : FALSE)

#define TAKE_CONTROL_MESSAGE_GET_CLIENTID(pBuf) \
    ((TakeControlMessage*)(pBuf))->clientId

/* Same structure used for both server and client messages */

typedef struct setwindowtitlesmessage_struct {
    CARD8 	msgType;
    CARD8       pad;
    CARD16      strLen;
    /* Followed by strLen bytes */
} SetWindowTitlesMessage;

#define SET_WINDOW_TITLES_MESSAGE_SIZE sizeof(SetWindowTitlesMessage)

#define SET_WINDOW_TITLES_MESSAGE_SET_TYPE(pBuf, mType) \
    ((SetWindowTitlesMessage *)(pBuf))->msgType = (mType)

#define SET_WINDOW_TITLES_MESSAGE_GET_STRLEN(pBuf) \
    ((((SetWindowTitlesMessage*)(pBuf))->strLen))

#define SET_WINDOW_TITLES_MESSAGE_SET_STRLEN(pBuf, len) \
    ((SetWindowTitlesMessage *)(pBuf))->strLen = (len)

/* Same structure used for both server and client messages */

typedef struct movewindowmessage_struct {
    CARD8 	msgType;
    CARD8       pad;
    CARD16      x;
    CARD32      clientId;
    CARD32      wid;
    CARD16      y;
} MoveWindowMessage;

#define MOVE_WINDOW_MESSAGE_SIZE sizeof(MoveWindowMessage)

#define MOVE_WINDOW_MESSAGE_SET_TYPE(pBuf, mType) \
    ((MoveWindowMessage *)(pBuf))->msgType = (mType)

#define MOVE_WINDOW_MESSAGE_GET_X(pBuf) \
    ((((MoveWindowMessage*)(pBuf))->x))

#define MOVE_WINDOW_MESSAGE_SET_X(pBuf, xval) \
    ((MoveWindowMessage *)(pBuf))->x = (xval)

#define MOVE_WINDOW_MESSAGE_GET_CLIENT_ID(pBuf) \
    ((((MoveWindowMessage*)(pBuf))->clientId))

#define MOVE_WINDOW_MESSAGE_SET_CLIENT_ID(pBuf, cid) \
    ((MoveWindowMessage *)(pBuf))->clientId = (cid)

#define MOVE_WINDOW_MESSAGE_GET_WID(pBuf) \
    ((((MoveWindowMessage*)(pBuf))->wid))

#define MOVE_WINDOW_MESSAGE_SET_WID(pBuf, widval) \
    ((MoveWindowMessage *)(pBuf))->wid = (widval)

#define MOVE_WINDOW_MESSAGE_GET_Y(pBuf) \
    ((((MoveWindowMessage*)(pBuf))->y))

#define MOVE_WINDOW_MESSAGE_SET_Y(pBuf, yval) \
    ((MoveWindowMessage *)(pBuf))->y = (yval)

/* Same structure used for both server and client messages */

typedef struct resizewindowmessage_struct {
    CARD8 	msgType;
    CARD8       pad1;
    CARD16      pad2;
    CARD32      clientId;
    CARD32      wid;
    CARD32      width;
    CARD32      height;
} ResizeWindowMessage;

#define RESIZE_WINDOW_MESSAGE_SIZE sizeof(ResizeWindowMessage)

#define RESIZE_WINDOW_MESSAGE_SET_TYPE(pBuf, mType) \
    ((ResizeWindowMessage *)(pBuf))->msgType = (mType)

#define RESIZE_WINDOW_MESSAGE_GET_CLIENT_ID(pBuf) \
    ((((ResizeWindowMessage*)(pBuf))->clientId))

#define RESIZE_WINDOW_MESSAGE_SET_CLIENT_ID(pBuf, cid) \
    ((ResizeWindowMessage *)(pBuf))->CLIENT_ID = (cid)

#define RESIZE_WINDOW_MESSAGE_GET_WID(pBuf) \
    ((((ResizeWindowMessage*)(pBuf))->wid))

#define RESIZE_WINDOW_MESSAGE_SET_WID(pBuf, widval) \
    ((ResizeWindowMessage *)(pBuf))->wid = (widval)

#define RESIZE_WINDOW_MESSAGE_GET_WIDTH(pBuf) \
    ((((ResizeWindowMessage*)(pBuf))->width))

#define RESIZE_WINDOW_MESSAGE_SET_WIDTH(pBuf, w) \
    ((ResizeWindowMessage *)(pBuf))->width = (w)

#define RESIZE_WINDOW_MESSAGE_GET_HEIGHT(pBuf) \
    ((((ResizeWindowMessage*)(pBuf))->height))

#define RESIZE_WINDOW_MESSAGE_SET_HEIGHT(pBuf, h) \
    ((ResizeWindowMessage *)(pBuf))->height = (h)

typedef struct hellomessage_struct {
    CARD8 	msgType;
} HelloMessage;

#define MOVE_WINDOW_MESSAGE_SIZE sizeof(MoveWindowMessage)

/*
** Server to Client Messages
*/

#define SERVER_MESSAGE_TYPE_CREATE_WINDOW    		0
#define SERVER_MESSAGE_TYPE_DESTROY_WINDOW   		1
#define SERVER_MESSAGE_TYPE_SHOW_WINDOW      		2
#define SERVER_MESSAGE_TYPE_CONFIGURE_WINDOW 		3
#define SERVER_MESSAGE_TYPE_POSITION_WINDOW  		4
#define SERVER_MESSAGE_TYPE_WINDOW_SET_DECORATED  	5
#define SERVER_MESSAGE_TYPE_WINDOW_SET_BORDER_WIDTH  	6
#define SERVER_MESSAGE_TYPE_BEEP	 		7
#define SERVER_MESSAGE_TYPE_DISPLAY_PIXELS	 	8
#define SERVER_MESSAGE_TYPE_COPY_AREA    	 	9
#define SERVER_MESSAGE_TYPE_CONTROLLER_STATUS	 	10
#define SERVER_MESSAGE_TYPE_DISPLAY_CURSOR 		11
#define SERVER_MESSAGE_TYPE_MOVE_CURSOR                 12
#define SERVER_MESSAGE_TYPE_SHOW_CURSOR                 13
#define SERVER_MESSAGE_TYPE_SET_WINDOW_TITLES           14
#define SERVER_MESSAGE_TYPE_WELCOME                 	15
#define SERVER_MESSAGE_TYPE_PING                 	16

typedef struct {
    CARD8 	msgType;
    CARD8 	decorated;		
    CARD16 	borderWidth;
    CARD32 	wid;
    CARD16 	x;
    CARD16 	y;
    CARD32 	wAndBorder;  /* Includes 2 * bw */
    CARD32 	hAndBorder;  /* Includes 2 * bw */
} CreateWindowMessage;

#define CREATE_WINDOW_MESSAGE_SIZE sizeof(CreateWindowMessage)

#define CREATE_WINDOW_MESSAGE_SET_TYPE(pBuf, mType) \
    ((CreateWindowMessage *)(pBuf))->msgType = (mType)

#define CREATE_WINDOW_MESSAGE_SET_DECORATED(pBuf, decor) \
    ((CreateWindowMessage *)(pBuf))->decorated = (decor)

#define CREATE_WINDOW_MESSAGE_SET_BORDER_WIDTH(pBuf, bw) \
    ((CreateWindowMessage *)(pBuf))->borderWidth = (bw)

#define CREATE_WINDOW_MESSAGE_SET_WID(pBuf, windowId) \
    ((CreateWindowMessage *)(pBuf))->wid = (windowId)

#define CREATE_WINDOW_MESSAGE_SET_X(pBuf, xval) \
    ((CreateWindowMessage *)(pBuf))->x = (xval)

#define CREATE_WINDOW_MESSAGE_SET_Y(pBuf, yval) \
    ((CreateWindowMessage *)(pBuf))->y = (yval)

#define CREATE_WINDOW_MESSAGE_SET_WANDBORDER(pBuf, wandb) \
    ((CreateWindowMessage *)(pBuf))->wAndBorder = (wandb)

#define CREATE_WINDOW_MESSAGE_SET_HANDBORDER(pBuf, handb) \
    ((CreateWindowMessage *)(pBuf))->hAndBorder = (handb)

/* Same structure used for both server and client messages */

typedef struct {
    CARD8 	msgType;			
    CARD8 	pad1;		
    CARD16 	pad2;
    CARD32 	wid;
} DestroyWindowMessage;

#define DESTROY_WINDOW_MESSAGE_SIZE sizeof(DestroyWindowMessage)

#define DESTROY_WINDOW_MESSAGE_SET_TYPE(pBuf, mType) \
    ((DestroyWindowMessage *)(pBuf))->msgType = (mType)

#define DESTROY_WINDOW_MESSAGE_GET_WID(pBuf) \
    ((((DestroyWindowMessage*)(pBuf))->wid))

#define DESTROY_WINDOW_MESSAGE_SET_WID(pBuf, windowId) \
    ((DestroyWindowMessage *)(pBuf))->wid = (windowId)

typedef struct {
    CARD8 	msgType;
    CARD8 	show;		
    CARD16 	pad;
    CARD32 	wid;
} ShowWindowMessage;

#define SHOW_WINDOW_MESSAGE_SIZE sizeof(ShowWindowMessage)

#define SHOW_WINDOW_MESSAGE_SET_TYPE(pBuf, mType) \
    ((ShowWindowMessage *)(pBuf))->msgType = (mType)

#define SHOW_WINDOW_MESSAGE_SET_WID(pBuf, windowId) \
    ((ShowWindowMessage *)(pBuf))->wid = (windowId)

#define SHOW_WINDOW_MESSAGE_SET_SHOW(pBuf, showit) \
    ((ShowWindowMessage *)(pBuf))->show = (showit)

typedef struct {
    CARD8 	msgType;
    CARD8 	pad1;
    CARD16	pad2;
    CARD32      clientId;
    CARD32 	wid;
    CARD16 	x;
    CARD16 	y;
    CARD32 	wAndBorder;    /* Includes 2 * bw */
    CARD32 	hAndBorder;    /* Includes 2 * bw */
    CARD32 	sibid;
} ConfigureWindowMessage;

#define CONFIGURE_WINDOW_MESSAGE_SIZE sizeof(ConfigureWindowMessage)

#define CONFIGURE_WINDOW_MESSAGE_SET_TYPE(pBuf, mType) \
    ((ConfigureWindowMessage *)(pBuf))->msgType = (mType)

#define CONFIGURE_WINDOW_MESSAGE_SET_CLIENT_ID(pBuf, cid) \
    ((ConfigureWindowMessage *)(pBuf))->clientId = (clientId)

#define CONFIGURE_WINDOW_MESSAGE_SET_WID(pBuf, windowId) \
    ((ConfigureWindowMessage *)(pBuf))->wid = (windowId)

#define CONFIGURE_WINDOW_MESSAGE_SET_X(pBuf, xval) \
    ((ConfigureWindowMessage *)(pBuf))->x = (xval)

#define CONFIGURE_WINDOW_MESSAGE_SET_Y(pBuf, yval) \
    ((ConfigureWindowMessage *)(pBuf))->y = (yval)

#define CONFIGURE_WINDOW_MESSAGE_SET_WANDBORDER(pBuf, wandb) \
    ((ConfigureWindowMessage *)(pBuf))->wAndBorder = (wandb)

#define CONFIGURE_WINDOW_MESSAGE_SET_HANDBORDER(pBuf, handb) \
    ((ConfigureWindowMessage *)(pBuf))->hAndBorder = (handb)

#define CONFIGURE_WINDOW_MESSAGE_SET_SIBID(pBuf, siblingId) \
    ((ConfigureWindowMessage *)(pBuf))->sibid = (siblingId)

typedef struct  {
    CARD8 	msgType;
    CARD8 	pad1;
    CARD16 	pad2;
    CARD32      clientId;
    CARD32 	wid;
    CARD16 	x;
    CARD16 	y;
} PositionWindowMessage;

#define POSITION_WINDOW_MESSAGE_SIZE sizeof(PositionWindowMessage)

#define POSITION_WINDOW_MESSAGE_SET_TYPE(pBuf, mType) \
    ((PositionWindowMessage *)(pBuf))->msgType = (mType)

#define POSITION_WINDOW_MESSAGE_SET_CLIENT_ID(pBuf, cid) \
    ((PositionWindowMessage *)(pBuf))->clientId = (clientId)

#define POSITION_WINDOW_MESSAGE_SET_WID(pBuf, windowId) \
    ((PositionWindowMessage *)(pBuf))->wid = (windowId)

#define POSITION_WINDOW_MESSAGE_SET_X(pBuf, xval) \
    ((PositionWindowMessage *)(pBuf))->x = (xval)

#define POSITION_WINDOW_MESSAGE_SET_Y(pBuf, yval) \
    ((PositionWindowMessage *)(pBuf))->y = (yval)

typedef struct {
    CARD8 	msgType;
    CARD8 	decorated;
    CARD16 	pad;
    CARD32 	wid;
} WindowSetDecoratedMessage;

#define WINDOW_SET_DECORATED_MESSAGE_SIZE sizeof(WindowSetDecoratedMessage)

#define WINDOW_SET_DECORATED_MESSAGE_SET_TYPE(pBuf, mType) \
    ((WindowSetDecoratedMessage *)(pBuf))->msgType = (mType)

#define WINDOW_SET_DECORATED_MESSAGE_SET_DECORATED(pBuf, decor) \
    ((WindowSetDecoratedMessage *)(pBuf))->decorated = (decor)

#define WINDOW_SET_DECORATED_MESSAGE_SET_WID(pBuf, windowId) \
    ((WindowSetDecoratedMessage *)(pBuf))->wid = (windowId)

typedef struct {
    CARD8 	msgType;
    CARD8 	pad;
    CARD16 	borderWidth;
    CARD32 	wid;
} WindowSetBorderWidthMessage;

#define WINDOW_SET_BORDER_WIDTH_MESSAGE_SIZE sizeof(WindowSetBorderWidthMessage)

#define WINDOW_SET_BORDER_WIDTH_MESSAGE_SET_TYPE(pBuf, mType) \
    ((WindowSetBorderWidthMessage *)(pBuf))->msgType = (mType)

#define WINDOW_SET_BORDER_WIDTH_MESSAGE_SET_BORDER_WIDTH(pBuf, bw) \
    ((WindowSetBorderWidthMessage *)(pBuf))->borderWidth = (bw)

#define WINDOW_SET_BORDER_WIDTH_MESSAGE_SET_WID(pBuf, windowId) \
    ((WindowSetBorderWidthMessage *)(pBuf))->wid = (windowId)

typedef struct {
    CARD8 	msgType;
} BeepMessage;

#define BEEP_MESSAGE_SIZE sizeof(BeepMessage)

#define BEEP_MESSAGE_SET_TYPE(pBuf, mType) \
    ((BeepMessage *)(pBuf))->msgType = (mType)

#define DISPLAY_PIXELS_ENCODING_UNCODED    0
#define DISPLAY_PIXELS_ENCODING_RLE24      1

/* 
** Note: Don't use xRectangle because x and y are constrained
** to fit in shorts because they are clipped to the screen.
*/

typedef struct {
    CARD16    x;
    CARD16    y;
    CARD16    width;
    CARD16    height;

    /* How pixels which follow are encoded */
    CARD32    encodingType;

} DirtyAreaRectRec, *DirtyAreaRectPtr;
    
#define DIRTY_AREA_RECT_SIZE sizeof(DirtyAreaRectRec)

typedef struct {
    CARD8 	msgType;
    CARD8 	pad;
    CARD16      numDirty;
    CARD32 	wid;
} DisplayPixelsMessage;

#define DISPLAY_PIXELS_MESSAGE_SIZE sizeof(DisplayPixelsMessage)

#define DISPLAY_PIXELS_MESSAGE_SET_TYPE(pBuf, mType) \
    ((DisplayPixelsMessage *)(pBuf))->msgType = (mType)

#define DISPLAY_PIXELS_MESSAGE_SET_NUM_DIRTY(pBuf, n) \
    ((DisplayPixelsMessage *)(pBuf))->numDirty = (n)

#define DISPLAY_PIXELS_MESSAGE_SET_WID(pBuf, windowId) \
    ((DisplayPixelsMessage *)(pBuf))->wid = (windowId)

typedef struct {
    CARD8 	msgType;
    CARD8 	pad1;		
    CARD16 	pad2;
    CARD32 	wid;
    CARD32 	srcx;
    CARD32 	srcy;
    CARD32 	width;
    CARD32 	height;
    CARD32 	dstx;
    CARD32 	dsty;
} CopyAreaMessage;

#define COPY_AREA_MESSAGE_SIZE sizeof(CopyAreaMessage)

#define COPY_AREA_MESSAGE_SET_TYPE(pBuf, mType) \
    ((CopyAreaMessage *)(pBuf))->msgType = (mType)

#define COPY_AREA_MESSAGE_SET_WID(pBuf, windowId) \
    ((CopyAreaMessage *)(pBuf))->wid = (windowId)

#define COPY_AREA_MESSAGE_SET_SRCX(pBuf, sx) \
    ((CopyAreaMessage *)(pBuf))->srcx = (sx)

#define COPY_AREA_MESSAGE_SET_SRCY(pBuf, sy) \
    ((CopyAreaMessage *)(pBuf))->srcy = (sy)

#define COPY_AREA_MESSAGE_SET_WIDTH(pBuf, w) \
    ((CopyAreaMessage *)(pBuf))->width = (w)

#define COPY_AREA_MESSAGE_SET_HEIGHT(pBuf, h) \
    ((CopyAreaMessage *)(pBuf))->height = (h)

#define COPY_AREA_MESSAGE_SET_DSTX(pBuf, dx) \
    ((CopyAreaMessage *)(pBuf))->dstx = (dx)

#define COPY_AREA_MESSAGE_SET_DSTY(pBuf, dy) \
    ((CopyAreaMessage *)(pBuf))->dsty = (dy)

typedef struct controllerstatusmessage_struct {
    CARD8 	msgType;			
    CARD8       status;
    CARD16      pad;
    CARD32      clientId;
} ControllerStatusMessage;

/* The attempt of the specified client to take control has been refused */
#define CONTROLLER_STATUS_REFUSED        0

/* The specified client has lost control */
#define CONTROLLER_STATUS_LOST           1

/* The specified client has gained control */
#define CONTROLLER_STATUS_GAINED         2

#define CONTROLLER_STATUS_MESSAGE_SIZE sizeof(ControllerStatusMessage)

#define CONTROLLER_STATUS_MESSAGE_SET_TYPE(pBuf, mType) \
    ((ControllerStatusMessage *)(pBuf))->msgType = (mType)

#define CONTROLLER_STATUS_MESSAGE_SET_STATUS(pBuf, stat) \
    ((ControllerStatusMessage*)(pBuf))->status = (stat)

#define CONTROLLER_STATUS_MESSAGE_SET_CLIENTID(pBuf, cid) \
    ((ControllerStatusMessage*)(pBuf))->clientId = (cid)

typedef struct displaycursormessage_struct {

    CARD8 	msgType;			
    CARD8       pad1;
    CARD16      pad2;
    CARD16      width;
    CARD16      height;
    CARD16      xhot;
    CARD16      yhot;

    /* Followed by (width * height) 32-bit pixels */

} DisplayCursorMessage;

#define DISPLAY_CURSOR_MESSAGE_SIZE sizeof(DisplayCursorMessage)

#define DISPLAY_CURSOR_MESSAGE_SET_TYPE(pBuf, mType) \
    ((DisplayCursorMessage *)(pBuf))->msgType = (mType)

#define DISPLAY_CURSOR_MESSAGE_SET_WIDTH(pBuf, w) \
    ((DisplayCursorMessage *)(pBuf))->width = (w)

#define DISPLAY_CURSOR_MESSAGE_SET_HEIGHT(pBuf, h) \
    ((DisplayCursorMessage *)(pBuf))->height = (h)

#define DISPLAY_CURSOR_MESSAGE_SET_XHOT(pBuf, xh) \
    ((DisplayCursorMessage *)(pBuf))->xhot = (xh)

#define DISPLAY_CURSOR_MESSAGE_SET_YHOT(pBuf, yh) \
    ((DisplayCursorMessage *)(pBuf))->yhot = (yh)

typedef struct movecursormessage_struct {
    CARD8 	msgType;			
    CARD8       pad1;
    CARD16      pad2;
    CARD32      wid;
    CARD32      x;
    CARD32      y;
} MoveCursorMessage;

#define MOVE_CURSOR_MESSAGE_SIZE sizeof(MoveCursorMessage)

#define MOVE_CURSOR_MESSAGE_SET_TYPE(pBuf, mType) \
    ((MoveCursorMessage *)(pBuf))->msgType = (mType)

#define MOVE_CURSOR_MESSAGE_SET_WID(pBuf, windowId) \
    ((MoveCursorMessage *)(pBuf))->wid = (windowId)

#define MOVE_CURSOR_MESSAGE_SET_X(pBuf, cx) \
    ((MoveCursorMessage *)(pBuf))->x = (cx)

#define MOVE_CURSOR_MESSAGE_SET_Y(pBuf, cy) \
    ((MoveCursorMessage *)(pBuf))->y = (cy)

typedef struct showcursormessage_struct {
    CARD8 	msgType;			
    CARD8       show;
} ShowCursorMessage;

#define SHOW_CURSOR_MESSAGE_SIZE sizeof(ShowCursorMessage)

#define SHOW_CURSOR_MESSAGE_SET_TYPE(pBuf, mType) \
    ((ShowCursorMessage *)(pBuf))->msgType = (mType)

#define SHOW_CURSOR_MESSAGE_SET_SHOW(pBuf, showit) \
    ((ShowCursorMessage *)(pBuf))->show = (showit)

typedef struct welcomessage_struct {
    CARD8 	msgType;			
    CARD8       pad1;
    CARD16      pad2;
    CARD32      clientId;
} WelcomeMessage;

#define WELCOME_MESSAGE_SIZE sizeof(WelcomeMessage)

#define WELCOME_MESSAGE_SET_TYPE(pBuf, mType) \
    ((WelcomeMessage *)(pBuf))->msgType = (mType)

#define WELCOME_MESSAGE_SET_CLIENT_ID(pBuf, cid) \
    ((WelcomeMessage *)(pBuf))->clientId = (cid)

#endif /* PROTOCOL_H */
