
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

Copyright 1985, 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

******************************************************************/

#include "inputstr.h"
#include "remwin.h"
#include "protocol.h"

/*
** Each time we grant control to a new controller, we need
** to make sure that if control was granted while the previous
** controller had any keys or buttons down that we start out the 
** keyboard state and button state with a clean state.

*/

static void
initKeyboardState (ScreenPtr pScreen)
{
    DeviceIntPtr pKeyboard = inputInfo.keyboard;
    unsigned long when = GetTimeInMillis();
    xEvent event;
    int i, k;

    for (i = 0; i < DOWN_LENGTH; i++) {
	if (pKeyboard->key->down[i] == 0) {
	    continue;
	}
	for (k = 0; k < 8; k++) {
	    int mask = 1 << k;
	    if (pKeyboard->key->down[i] & mask) {
		event.u.u.type = KeyRelease;
		event.u.u.detail = (i << 3) | k;
		event.u.keyButtonPointer.time = when;
		(*pKeyboard->public.processInputProc)(&event, pKeyboard, 1);
	    }
	}
    }
}

static void
initPointerState (ScreenPtr pScreen) 
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    DeviceIntPtr pPointer = inputInfo.pointer;
    xEvent event;
    int button;

    for (button = 0; button < 5; button++) {
	int mask = 1 << button;
	if (pScrPriv->controllerButtonMask & mask) {
	    event.u.u.type = ButtonRelease;
	    event.u.u.detail = button + 1;
	    event.u.keyButtonPointer.time = GetTimeInMillis();
	    (*pPointer->public.processInputProc)(&event, pPointer, 1);
	}
    }
}

static void
rwGrantControl (ScreenPtr pScreen, int clientId)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);

    /* Are we granting to someone who already has control? */
    if (pScrPriv->controller == clientId) {
	return;
    }

    pScrPriv->controller = clientId;

    initKeyboardState(pScreen);
    initPointerState(pScreen);

    /* TODO: what else? */
}

/*
** Notify everyone that the current controller has lost control
*/

static Bool
notifyControlLost (ScreenPtr pScreen)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[CONTROLLER_STATUS_MESSAGE_SIZE];
    int losingClient;
    int n;

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	ErrorF("Connection lost.\n");
	return FALSE;
    }

    losingClient = pScrPriv->controller;
    swapl(&losingClient, n);

    CONTROLLER_STATUS_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_CONTROLLER_STATUS);
    CONTROLLER_STATUS_MESSAGE_SET_STATUS(buf, CONTROLLER_STATUS_LOST);
    CONTROLLER_STATUS_MESSAGE_SET_CLIENTID(buf, losingClient);

    if (!RWCOMM_BUFFER_WRITE(pComm, buf, CONTROLLER_STATUS_MESSAGE_SIZE)) {
	ErrorF("Write to connection failed.\n");
	return FALSE;
    }

    return TRUE;
}

/*
** Notify everyone that the current controller has gained control
*/

static Bool
notifyControlGained (ScreenPtr pScreen)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[CONTROLLER_STATUS_MESSAGE_SIZE];
    int gainingClient;
    int n;

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	ErrorF("Connection lost.\n");
	return FALSE;
    }

    gainingClient = pScrPriv->controller;
    swapl(&gainingClient, n);

    CONTROLLER_STATUS_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_CONTROLLER_STATUS);
    CONTROLLER_STATUS_MESSAGE_SET_STATUS(buf, CONTROLLER_STATUS_GAINED);
    CONTROLLER_STATUS_MESSAGE_SET_CLIENTID(buf, gainingClient);

    if (!RWCOMM_BUFFER_WRITE(pComm, buf, CONTROLLER_STATUS_MESSAGE_SIZE)) {
	ErrorF("Write to connection failed.\n");
	return FALSE;
    }

    return TRUE;
}


/*
** Notify the given client that its request for control is refused.
*/

static Bool
notifyControlRefused (ScreenPtr pScreen, int clientId) 
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[CONTROLLER_STATUS_MESSAGE_SIZE];
    int refusedClient;
    int n;

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	ErrorF("Connection lost.\n");
	return FALSE;
    }

    refusedClient = pScrPriv->controller;
    swapl(&refusedClient, n);

    CONTROLLER_STATUS_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_CONTROLLER_STATUS);
    CONTROLLER_STATUS_MESSAGE_SET_STATUS(buf, CONTROLLER_STATUS_REFUSED);
    CONTROLLER_STATUS_MESSAGE_SET_CLIENTID(buf, refusedClient);

    if (!RWCOMM_BUFFER_WRITE_TO_CLIENT(pComm, clientId, 
			  buf, CONTROLLER_STATUS_MESSAGE_SIZE)) {
	ErrorF("Write to connection failed.\n");
	return FALSE;
    }

    return TRUE;
}

void
rwTakeControl (ScreenPtr pScreen, int clientId, Bool steal)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);

    /* If client already has control do nothing */
    if (pScrPriv->controller == clientId) {
	return;
    }

    if (pScrPriv->controller == -1 || steal) {

	/* Nobody has control or client is stealing -- Grant control */

	if (!notifyControlLost(pScreen)) {
	    ErrorF("Cannot notify clients that control is lost.\n");
	    return;
	}

	rwGrantControl(pScreen, clientId);
	
	if (!notifyControlGained(pScreen)) {
	    ErrorF("Cannot notify clients that control is gained.\n");
	    return;
	}

    } else {
	if (!notifyControlRefused(pScreen, clientId)) {
	    ErrorF("Cannot notify client %d that control is refused.\n", 
		   clientId);
	    return;
	}
    }
}

void
rwTakeControlInit (ScreenPtr pScreen)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);

    pScrPriv->controller = -1;
    pScrPriv->controllerButtonMask = 0;
    pScrPriv->configuringClient = -1;
}

