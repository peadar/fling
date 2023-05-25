#include "wmhack.h"
#include <X11/extensions/Xrandr.h>

std::ostream &
operator<<(std::ostream &os, const Geometry &m)
{
    return os << "{ w:" << m.size.width
                << ", h: " << m.size.height
                << ", x: " << m.x
                << ", y: " << m.y
                << " }";
}

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
    Geometry geom = getGeometry(w, &root);
    // Locate origin of this window in root.
    Status s = XTranslateCoordinates(display, w, root, 0, 0, &geom.x, &geom.y, &root);
    if (!s) {
       throw "can't translate geometry";
    }
    return geom;
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
    s = XTranslateCoordinates(display, win, winroot,  0, 0, &geom.x, &geom.y, &winroot);
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
    // Try XRandR
    int eventBase, eventError;
    if (XRRQueryExtension(display, &eventBase, &eventError) != 0) {
       int count = 0;
       auto xrandrMonitors = XRRGetMonitors(display, root, True, &count);
       if (count) {
           monitors.resize(count);
           for (int i = 0; i < count; ++i) {
              monitors[i].size.width = xrandrMonitors[i].width;
              monitors[i].size.height = xrandrMonitors[i].height;
              monitors[i].x = xrandrMonitors[i].x;
              monitors[i].y = xrandrMonitors[i].y;
           }
           XFree(xrandrMonitors);
           return;
       }
    }
    // try Xinerama.
    if (XineramaQueryExtension(display, &eventBase, &eventError) != 0) {
        int monitorCount;
        XineramaScreenInfo *xineramaMonitors = XineramaQueryScreens(display, &monitorCount);
        if (xineramaMonitors != 0) {
            monitors.resize(monitorCount);
            for (int i = 0; i < monitorCount; ++i) {
                monitors[i].size.width = xineramaMonitors[i].width;
                monitors[i].size.height = xineramaMonitors[i].height;
                monitors[i].x = xineramaMonitors[i].x_org;
                monitors[i].y = xineramaMonitors[i].y_org;
            }
            XFree(xineramaMonitors);
            return;
        }
    }
    /* Fallback case is a single monitor occupying the entire root window */
    monitors.resize(1);
    monitors[0] = rootGeom;
}

std::ostream &operator <<(std::ostream &os, const Range &r)
{
   os
      << "{ start: " << r.start
      << ", end: " << r.end
      << " }";
   return os;
}

std::ostream &operator <<(std::ostream &os, const PartialStrut &ps)
{
   os
      << "{ left: " << ps.left
      << ", right: " << ps.right
      << ", top: " << ps.top
      << ", bottom: " << ps.bottom
      << "{ rleft: " << ps.rleft
      << ", rright: " << ps.rright
      << ", rtop: " << ps.rtop
      << ", rbottom: " << ps.rbottom
      << " }";
   return os;
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
    Cursor c = XCreateFontCursor(display, XC_tcross);

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

long
X11Env::desktopForWindow(Window win) const
{
    Atom actualType;
    int actualFormat;
    unsigned long itemCount;
    unsigned long afterBytes;
    unsigned char *prop;
    long rv = -1;
    
    auto rc = XGetWindowProperty(display, win, NetWmDesktop, 0,
            std::numeric_limits<long>::max(), False, Cardinal,
            &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
    if  (rc == 0) {
        if (itemCount == 1)
            rv = *(long *)prop;
        XFree(prop);
    }
    return rv;
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
    // Things like gmrun will exit just after they execute the command they are
    // asked to run. Give them time to go away before finding the active
    // window, or else we just end up flinging the dialog box they present
    usleep(500000);
    int rc = XGetWindowProperty(display, root, NetActiveWindow,
            0, std::numeric_limits<long>::max(), False, AWindow,
            &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
    // XXX: xfce strangely has two items here, second appears to be zero.
    Window rv = 0;
    if (rc == 0) {
        if (actualFormat == 32 && itemCount >= 1)
            rv = *(Window *)prop;
        XFree(prop);
    }
    return rv;
}

void
X11Env::updateState(Window win, const Atom stateitem, StateUpdateAction action) const
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
    ec.data.l[0] = action;
    ec.data.l[1] = stateitem;
    ec.data.l[2] = stateitem == NetWmStateMaximizedHoriz ? NetWmStateMaximizedVert : 0 ;
    ec.data.l[3] = 1;
    if (!XSendEvent(display, root, False, SubstructureRedirectMask|SubstructureNotifyMask, &e))
        std::cerr << "can't go fullscreen" << std::endl;
    XSync(display, False);
}
