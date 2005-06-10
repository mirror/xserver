/* $XFree86$ */

#ifndef _XF86_USL_XQUEUE_H_
#define _XF86_USL_XQUEUE_H_

typedef struct _USLMseRec {
  int	wheel_res;
} USLMseRec, *USLMsePtr;

extern int XqMseOnOff (InputInfoPtr pInfo, int on);
extern int XqKbdOnOff (InputInfoPtr pInfo, int on);

#endif
