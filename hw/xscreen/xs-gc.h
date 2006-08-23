#ifndef _XS_GC_INCL_ 
#define _XS_GC_INCL_ 

typedef struct {
  XCBGCONTEXT gc;
} XscreenPrivGC;

extern int xsGCPrivateIndex;

#define XS_GC_PRIV(pGC) \
  ((XscreenPrivGC *)((pGC)->devPrivates[xsGCPrivateIndex].ptr))

Bool xsCreateGC(GCPtr pGC);
void xsValidateGC(GCPtr pGC, unsigned long changes, DrawablePtr pDrawable);
void xsChangeGC(GCPtr pGC, unsigned long mask);
void xsCopyGC(GCPtr pGCSrc, unsigned long mask, GCPtr pGCDst);
void xsDestroyGC(GCPtr pGC);
void xsChangeClip(GCPtr pGC, int type, pointer pValue, int nRects);
void xsDestroyClip(GCPtr pGC);
void xsDestroyClipHelper(GCPtr pGC);
void xsCopyClip(GCPtr pGCDst, GCPtr pGCSrc);

#endif 
