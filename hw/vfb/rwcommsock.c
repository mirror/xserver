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

#include <unistd.h>
#include <errno.h>
#include <X11/Xproto.h>
#include "dix-config.h"
#include "remwin.h"
#include "rwcomm.h"
#include "rwcommsock.h"
#include "protocol.h"
#include "opaque.h"
#include <arpa/inet.h>
#include <fcntl.h>

static void rwcommsockDisconnect (RwcommPtr pComm);
static int WriteFully (int sock, char *pbuf, int bufLen);
static int ReadFully (int sock, char *pbuf, int bufLen);

/*
** Disconnect all activate connections and free the comm module.
*/

static void
rwcommsockDestroy (RwcommPtr pComm) 
{
    rwcommsockDisconnect(pComm);
    xfree(pComm);
}

/* TODO: notyet
static Bool firstClient = TRUE;
*/

/*
** Called when a new client connection is received.
*/

static void
rwcommsockConnect (RwcommPtr pComm)
{
    RwcommsockPtr pCommsock = (RwcommsockPtr) pComm;
    ScreenPtr pScreen = pCommsock->pScreen;

    pCommsock->isConnected = TRUE;

    rwoutTimerCreate(pScreen);

    rwTakeControlInit(pScreen);

    // TODO: DS: clientptr is an opaque handle to the client's sessionid
    rwoutSyncClientOnConnect(pScreen, NULL);

    /* TODO: notyet
    if (firstClient) {
	// By this time we know that composite has been enabled and
	// it is safe to discard frame buffer memory
	ErrorF("Discarding fb memory");
	ddxGiveUp();
	firstClient = FALSE;
    }
    */
}

/*
** Disconnect all connections.
*/

static void
rwcommsockDisconnect (RwcommPtr pComm) 
{
    RwcommsockPtr pCommsock = (RwcommsockPtr) pComm;

    FD_CLR(pCommsock->socket, &pCommsock->fdSetPoll);
    RemoveEnabledDevice(pCommsock->socket);
    close(pCommsock->socket);

    if (pCommsock->sockServer > 0) {
    	if (close(pCommsock->sockServer)) {
	    ErrorF("rwcommsockDisconnect failed\n");
	    return;
	}
    }

    pCommsock->isConnected = FALSE;
    rwoutTimerDestroy(pCommsock->pScreen);

    ErrorF("Client disconnected\n");
}

static Bool
rwcommsockIsConnected (RwcommPtr pComm) 
{
    RwcommsockPtr pCommsock = (RwcommsockPtr) pComm;
    return pCommsock->isConnected;
}

/* 
** Broadcast buffer to all clients 
*/

static Bool
rwcommsockBufferWrite (RwcommPtr pComm, char *pbuf, int bufLen) 
{
    RwcommsockPtr pCommsock = (RwcommsockPtr) pComm;
    int ret;

    ret = WriteFully(pCommsock->socket, pbuf, bufLen);
    if (ret < 0) {
	ErrorF("rwcommsockBufferWrite: IO error\n");
	rwcommsockDestroy((RwcommPtr)pCommsock);
	return FALSE;
    }

#ifdef VERBOSE
    { int i;

      bufLen = (bufLen > 50) ? 50 : bufLen;

      ErrorF("Broadcast: ");
      for (i = 0; i < bufLen; i++) {
	  ErrorF("0x%x ", ((int)buf[i]) & 0xff);
      }
      ErrorF("\n");
    }
#endif /* VERBOSE */

    return TRUE;
}

/* 
** Unicast buffer to specified client.
*/

static Bool
rwcommsockBufferWriteToClient (RwcommPtr pComm, int clientId, char *buf, int bufLen) 
{
    RwcommsockPtr pCommsock = (RwcommsockPtr) pComm;
    int ret;

    /* 
    ** Note: we can't do unicast in the socket implementation. We can
    ** only do broadcast. So clients will need to ignore this if necessary.
    */
    ret = WriteFully(pCommsock->socket, buf, bufLen);
    if (ret <= 0) {
	ErrorF("rwcommsockBufferWriteToClient: IO error\n");
	rwcommsockDestroy((RwcommPtr)pCommsock);
	return FALSE;
    }

#ifdef VERBOSE
    { int i;
      
      bufLen = (bufLen > 50) ? 50 : bufLen;

      ErrorF("Unicast to client %d: ", clientId);
      for (i = 0; i < bufLen; i++) {
	  ErrorF("0x%x ", ((int)buf[i]) & 0xff);
      }
      ErrorF("\n");
    }
#endif /* VERBOSE */

    return TRUE;
}

static int
rwcommsockNextMessageTypeRead (RwcommPtr pComm) 
{
    RwcommsockPtr pCommsock = (RwcommsockPtr) pComm;
    char msgType;
    int ret;

    ret = ReadFully(pCommsock->socket, &msgType, 1);
    if (ret <= 0) {
	if (ret != 0) {
	    ErrorF("rwcommsockNextMessageTypeRead: IO error\n");
	}
	rwcommsockDestroy((RwcommPtr)pCommsock);
	return CLIENT_MESSAGE_TYPE_INVALID;
    }

    return (int) msgType;
}

static Bool
rwcommsockNextMessageBufferRead (RwcommPtr pComm, char *pbuf, int readLen) 
{
    RwcommsockPtr pCommsock = (RwcommsockPtr) pComm;
    int ret;

    ret = ReadFully(pCommsock->socket, pbuf, readLen);
    if (ret <= 0) {
	if (ret != 0) {
	    ErrorF("rwcommsockNextMessageBufferRead: IO error\n");
	}
	rwcommsockDestroy((RwcommPtr)pCommsock);
	return FALSE;
    }

    return TRUE;
}

static void
rwcommsockClientMessagePoll (RwcommPtr pComm, fd_set *pReadMask)
{
    RwcommsockPtr pCommsock = (RwcommsockPtr) pComm;
    int numFdsSet;
    fd_set tempFds;
    struct timeval waittime;

    memcpy((char *)&tempFds, (char *)&pCommsock->fdSetPoll, sizeof(fd_set));
    waittime.tv_usec = 0;
    waittime.tv_sec = 0;
    numFdsSet = select(pCommsock->fdMax + 1, &tempFds, NULL, NULL, &waittime);
    if (numFdsSet <= 0) {
	if (numFdsSet == 0) return;
	if (errno != EINTR) {
	    ErrorF("rwcommsockClientMessagePoll: select error\n");
	}
	return;
    }

    /* 
    ** We only listen for a single connection. Only accept a new connection
    ** if we aren't already connected.
    */
    if (!pCommsock->isConnected &&
	pCommsock->sockServer != -1 && 
	FD_ISSET(pCommsock->sockServer, pReadMask)) {

	int newSocket;
	struct sockaddr_in addrDummy;
	socklen_t lenDummy = sizeof(struct sockaddr_in);

	if ((newSocket = accept(pCommsock->sockServer,
				(struct sockaddr *)&addrDummy, &lenDummy)) < 0) {
	    perror("rwcommsockClientMessagePoll: accept");
	    return;
	}

	/*
	if (fcntl(newSocket, F_SETFL, O_NONBLOCK) < 0) {
	    perror("rwcommsockClientMessagePoll: fcntl");
	    close(newSocket);
	    return;
	}
	*/

	pCommsock->fdMax = (newSocket > pCommsock->fdMax) ? newSocket :pCommsock->fdMax;
	FD_SET(newSocket, &pCommsock->fdSetPoll);
	AddEnabledDevice(newSocket);

	pCommsock->socket = newSocket;
	RWCOMM_CONNECT((RwcommPtr)pCommsock); 

	ErrorF("New client connected\n");

	numFdsSet--;
	if (numFdsSet == 0) {
	    return;
	}
    }

    /* Call input handler when the socket has data */
    if (FD_ISSET(pCommsock->socket, pReadMask) && 
	FD_ISSET(pCommsock->socket, &pCommsock->fdSetPoll)) {
	rwinHandler(pCommsock->pScreen);
    }
}

/*
** Create the comm module and accept connections.
*/

RwcommPtr
rwcommsockCreate (ScreenPtr pScreen) 
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommsockPtr pCommsock;
    struct sockaddr_in sockAddr;
    int val = 1;

    pCommsock = (RwcommsockPtr) xalloc(sizeof(RwcommsockRec));
    if (pCommsock == NULL) {
	return NULL;
    }

    pCommsock->pScreen = pScreen;
    pCommsock->sockServer = -1;
    pCommsock->isConnected = FALSE;

    pCommsock->funcs.destroy = rwcommsockDestroy;
    pCommsock->funcs.connect = rwcommsockConnect;
    pCommsock->funcs.disconnect = rwcommsockDisconnect;
    pCommsock->funcs.isConnected = rwcommsockIsConnected;
    pCommsock->funcs.bufferWrite = rwcommsockBufferWrite;
    pCommsock->funcs.bufferWriteToClient = rwcommsockBufferWriteToClient;
    pCommsock->funcs.nextMessageTypeRead = rwcommsockNextMessageTypeRead;
    pCommsock->funcs.nextMessageBufferRead = rwcommsockNextMessageBufferRead;
    pCommsock->funcs.clientMessagePoll = rwcommsockClientMessagePoll;

    pScrPriv->pComm = (RwcommPtr) pCommsock;

    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(RWCOMMSOCK_SERVER_PORT + atoi(display));
    sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    pCommsock->sockServer = socket(AF_INET, SOCK_STREAM, 0);
    if (pCommsock->sockServer < 0) {
	xfree(pCommsock);
	return NULL;
    }

    if (setsockopt(pCommsock->sockServer, SOL_SOCKET, SO_REUSEADDR, (char *)&val, 4) < 0) {
	close(pCommsock->sockServer);
	xfree(pCommsock);
	return NULL;
    }

    if (bind(pCommsock->sockServer, 
	     (struct sockaddr *)&sockAddr, sizeof(struct sockaddr_in)) < 0) {
	close(pCommsock->sockServer);
	xfree(pCommsock);
	return NULL;
    }

    if (listen(pCommsock->sockServer, 1) < 0) {
	close(pCommsock->sockServer);
	xfree(pCommsock);
	return NULL;
    }

    FD_ZERO(&pCommsock->fdSetPoll);
    FD_SET(pCommsock->sockServer, &pCommsock->fdSetPoll);
    pCommsock->fdMax = pCommsock->sockServer;
    AddEnabledDevice(pCommsock->sockServer);

    return (RwcommPtr)pCommsock;
}

static int
ReadFully (int sock, char *pbuf, int bufLen)
{
    int bytes_read;

    errno = 0;

    bytes_read = read(sock, pbuf, bufLen);    
    while (bytes_read != bufLen) {
	if (bytes_read > 0) {
	    bufLen -= bytes_read;
	    pbuf += bytes_read;
	} else if (bytes_read == 0) {
	    /* Read failed because of end of file! */
	    errno = EPIPE;
	    return -1;
	} else if (errno == EWOULDBLOCK ||
		   errno == EAGAIN) {
	    int ret;
	    fd_set r_mask;

	    FD_ZERO(&r_mask);
	    for (;;) {
		FD_SET(sock, &r_mask);
		ret = select(sock + 1, &r_mask, NULL, NULL, NULL);
		if (ret == -1 && 
		    errno != EINTR) {
		    return -1;
		}
		if (ret <= 0) {
		    continue;
		}
		if (FD_ISSET(sock, &r_mask)) {
		    break;
		}
	    }
	    errno = 0;
	} else {
	    if (errno != EINTR) {
		return -1;
	    }
	}
        bytes_read = read(sock, pbuf, bufLen);    
    }

    return bufLen;
}

static int
WriteFully (int sock, char *pbuf, int bufLen)
{
    int todo = bufLen;
    int ret;

    while (bufLen != 0) {
	ret = write(sock, pbuf, todo);
	if (ret >= 0) {
	    pbuf += ret;
	    bufLen -= ret;
	    todo = bufLen;
	} else if (errno == EWOULDBLOCK ||
		   errno == EAGAIN) {
	    int nfound;
	    fd_set w_mask;

	    FD_ZERO(&w_mask);
	    for (;;) {
		FD_SET(sock, &w_mask);
		do {
		    nfound = select(sock + 1, NULL, &w_mask, NULL, NULL);
		    if (nfound < 0 && errno != EINTR) {
			return -1;
		    }
		} while (nfound <= 0);
	    }
	} else if (errno != EINTR) {
	    return -1;
	}
    }

    return bufLen;
}
