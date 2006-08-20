#ifndef _XS_GLOBALS_INCL_
#define _XS_GLOBALS_INCL_

#define MAXDEPTH 32

extern XCBConnection *xsConnection;
extern XCBDRAWABLE    xsDefaultDrawables[MAXDEPTH];
extern XCBDRAWABLE    xsDefaultWindow;
extern int            xsFontPrivateIndex;
extern int            xsGCPrivateIndex;
extern int            xsWindowPrivateIndex;

#endif
