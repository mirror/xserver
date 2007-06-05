
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

#ifndef RWCOMM_H
#define RWCOMM_H

#define RWCOMM_DESTROY(pComm) \
    (pComm)->funcs.destroy(pComm)

#define RWCOMM_CONNECT(pComm) \
    (pComm)->funcs.connect(pComm)

#define RWCOMM_DISCONNECT(pComm) \
    (pComm)->funcs.disconnect(pComm)

#define RWCOMM_IS_CONNECTED(pComm) \
    (pComm)->funcs.isConnected(pComm)

#define RWCOMM_BUFFER_WRITE(pComm, pBuf, bufLen) \
    (pComm)->funcs.bufferWrite((pComm), (pBuf), (bufLen))

#define RWCOMM_BUFFER_WRITE_TO_CLIENT(pComm, clientId, pBuf, bufLen) \
    (pComm)->funcs.bufferWriteToClient((pComm), (clientId), (pBuf), (bufLen))

#define RWCOMM_NEXT_MESSAGE_TYPE_READ(pComm) \
    (pComm)->funcs.nextMessageTypeRead(pComm)

#define RWCOMM_NEXT_MESSAGE_BUFFER_READ(pComm, buf, readLen) \
    (pComm)->funcs.nextMessageBufferRead((pComm), (buf), (readLen))

#define RWCOMM_CLIENT_MESSAGE_POLL(pComm, pReadMask) \
    (pComm)->funcs.clientMessagePoll((pComm), (pReadmask))

typedef struct rwcomm_rec *RwcommPtr;

typedef void (*RwcommFuncPtrDestroy)(RwcommPtr pComm);
typedef void (*RwcommFuncPtrConnect)(RwcommPtr pComm);
typedef void (*RwcommFuncPtrDisconnect)(RwcommPtr pComm);
typedef Bool (*RwcommFuncPtrIsConnected)(RwcommPtr pComm);
typedef Bool (*RwcommFuncPtrBufferWrite)(RwcommPtr pComm, 
					 char *buf, int bufLen);
typedef Bool (*RwcommFuncPtrBufferWriteToClient)(RwcommPtr pComm, 
					 int clientId, char *buf, int bufLen);
typedef int (*RwcommFuncPtrNextMessageTypeRead)(RwcommPtr pComm);
typedef int (*RwcommFuncPtrNextMessageBufferRead)(RwcommPtr pComm,
						  char *buf, int readLen);
typedef void (*RwcommFuncPtrClientMessagePoll)(RwcommPtr pComm, fd_set *pReadMask);

typedef struct rwcomm_funcptr_rec {
    RwcommFuncPtrDestroy		destroy;
    RwcommFuncPtrConnect		connect;
    RwcommFuncPtrDisconnect		disconnect;
    RwcommFuncPtrIsConnected            isConnected;
    RwcommFuncPtrBufferWrite            bufferWrite;
    RwcommFuncPtrBufferWriteToClient    bufferWriteToClient;
    RwcommFuncPtrNextMessageTypeRead    nextMessageTypeRead;
    RwcommFuncPtrNextMessageBufferRead  nextMessageBufferRead;
    RwcommFuncPtrClientMessagePoll      clientMessagePoll;
} RwcommFuncPtrRec, *RwcommFuncPtrPtr;

typedef struct rwcomm_rec {
    RwcommFuncPtrRec funcs;
} RwcommRec;

typedef pointer RwcommClientPtr;

#endif /* RWCOMM_H */
