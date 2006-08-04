WindowPtr xnestTrackWindow(XCBWINDOW w, WindowPtr pParent, int x, int y, int width, int height, int bw);
void xnestInsertWindow(WindowPtr pWin, WindowPtr pParent);
int xnestReparentWindow(register WindowPtr pWin, register WindowPtr pParent, int x, int y, ClientPtr client);
void DBG_xnestListWindows(XCBWINDOW w);

