#include "wmhack.h"

X11Env::X11Env(Display *display_)
    : display(display_)
    , root(XDefaultRootWindow(display))
    , rootGeom(getGeometry(root))
{
    detectMonitors();
}

Geometry 
X11Env::getGeometry(Window w) const
{
    Window root;
    return getGeometry(w, &root);
}

Geometry
X11Env::getGeometry(Window w, Window *root) const
{
    Geometry returnValue;
    unsigned int borderWidth;
    unsigned int depth;
    Status s = XGetGeometry(display, w, root,  &returnValue.x, &returnValue.y,
                &returnValue.size.width, &returnValue.size.height, &borderWidth, &depth);
    if (!s)
        throw "Can't get window geometry";
    return returnValue;
}

int
X11Env::monitorForWindow(Window win)
{
    Window winroot;
    Status s;
    Geometry geom = getGeometry(win, &winroot);
    s = XTranslateCoordinates(display, win, winroot,  geom.x, geom.y, &geom.x, &geom.y, &winroot);
    if (!s) {
        std::cerr << "Can't translate root window coordinates" << std::endl;
        return 0;
    }
    int midX = geom.x + geom.size.width / 2;
    int midY = geom.y + geom.size.height / 2;

    /*
     * XXX: really need to sort by area of window on the monitor:
     * centre may not be in any // monitor
     */
    for (size_t i = 0; i < monitors.size(); ++i) {
        Geometry &mon = monitors[i];
        if (midX >= mon.x && midX < mon.x + int(mon.size.width)
                && midY >= monitors[i].y && midY < mon.y + int(mon.size.height)) {
            return i;
        }
    }
    return 0;
}

void
X11Env::setGeometry(Window win, const Geometry &geom) const
{
    // Tell the WM where to put it.
    XEvent e;
    XClientMessageEvent &ec = e.xclient;
    ec.type = ClientMessage;
    ec.serial = 1;
    ec.send_event = True;
    ec.message_type = NetMoveResizeWindow;
    ec.window = win;
    ec.format = 32;
    ec.data.l[0] = 0xf0a;
    ec.data.l[1] = geom.x;
    ec.data.l[2] = geom.y;
    ec.data.l[3] = geom.size.width;
    ec.data.l[4] = geom.size.height;
    XSendEvent(display, root, False, SubstructureRedirectMask|SubstructureNotifyMask, &e);
    XSync(display, False);
}


void
X11Env::detectMonitors()
{
    // If xinerama is present, use it.
    int eventBase, eventError;
    if (XineramaQueryExtension(display, &eventBase, &eventError) == 0) {
        monitors.resize(1);
        monitors[0] = rootGeom;
    } else {
        int monitorCount;
        XineramaScreenInfo *xineramaMonitors = XineramaQueryScreens(display, &monitorCount);
        if (xineramaMonitors == 0) {
            std::cerr << "have xinerama, but can't get monitor info" << std::endl;
            exit(1);
        }
        monitors.resize(monitorCount);
        for (int i = 0; i < monitorCount; ++i) {
            monitors[i].size.width = xineramaMonitors[i].width;
            monitors[i].size.height = xineramaMonitors[i].height;
            monitors[i].x = xineramaMonitors[i].x_org;
            monitors[i].y = xineramaMonitors[i].y_org;
        }
        XFree(xineramaMonitors);
    }
}

void
PartialStrut::box(const X11Env &x11, Geometry &g)
{
    if (rtop.aligned(g.x, g.size.width) && top > g.y) {
        g.size.height -= top - g.y;
        g.y = top;
    }
    if (rleft.aligned(g.y, g.size.height) && left > g.x) {
        g.size.width -= left - g.x;
        g.x = left;
    }
    long winend = g.y + g.size.height;
    long strutend = x11.rootGeom.size.height - bottom;
    if (rbottom.aligned(g.x, g.size.width) && strutend < winend)
        g.size.height -= winend - strutend;
    winend = g.x + g.size.width;
    strutend = x11.rootGeom.size.width - right;
    if (rright.aligned(g.y, g.size.height) && strutend < winend)
        g.size.width -= winend - strutend;
}

Window
X11Env::pick()
{
    Window w = root;
    Cursor c = XCreateFontCursor(display, XC_question_arrow);

    if (XGrabPointer(display, root, False, ButtonPressMask|ButtonReleaseMask,
            GrabModeSync, GrabModeAsync, None, c, CurrentTime) != GrabSuccess) {
        throw "can't grab pointer";
    }

    for (bool done = false; !done;) {
        XEvent event;
        XAllowEvents(display, SyncPointer, CurrentTime);
        XWindowEvent(display, root, ButtonPressMask|ButtonReleaseMask, &event);
        switch (event.type) {
            case ButtonPress:
                if (event.xbutton.button == 1 && event.xbutton.subwindow != None)
                    w = event.xbutton.subwindow;
                break;
            case ButtonRelease:
                done = true;
                break;
        }
    }
    XUngrabPointer(display, CurrentTime);
    XFreeCursor(display, c);
    return XmuClientWindow(display, w);
}

Window
X11Env::active()
{
    // Find active window from WM.
    Atom actualType;
    int actualFormat;
    unsigned long itemCount;
    unsigned long afterBytes;
    unsigned char *prop;
    int rc = XGetWindowProperty(display, root, NetActiveWindow,
            0, std::numeric_limits<long>::max(), False, AWindow,
            &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
    // XXX: xfce strangely has two items here, second appears to be zero.
    if (rc != 0 || actualFormat != 32 || itemCount < 1) {
        std::cerr << "can't find active window";
        exit(1);
    }
    return *(Window *)prop;
}

void
X11Env::toggleFlag(Window win, const Atom toggle) const
{
    XEvent e;
    XClientMessageEvent &ec = e.xclient;
    memset(&e, 0, sizeof e);
    ec.type = ClientMessage;
    ec.serial = 1;
    ec.send_event = True;
    ec.message_type = NetWmState;
    ec.window = win;
    ec.format = 32;
    ec.data.l[0] = 2; //_NET_WM_STATE_TOGGLE;
    ec.data.l[1] = toggle;
    ec.data.l[2] = toggle == NetWmStateMaximizedHoriz ? NetWmStateMaximizedVert : 0 ;
    ec.data.l[3] = 1;
    if (!XSendEvent(display, root, False, SubstructureRedirectMask|SubstructureNotifyMask, &e))
        std::cerr << "can't go fullscreen" << std::endl;
    XSync(display, False);
}

bool
Geometry::operator < (const Geometry &rhs) const
{
    if (size < rhs.size)
        return true;
    if (x < rhs.x)
        return true;
    if (y < rhs.y)
        return true;
    return false;
}

bool
Size::operator < (const Size &rhs) const
{
    if (width < rhs.width)
        return true;
    if (height < rhs.height)
        return true;
    return false;
}
