#include <iostream>
#include <unistd.h>
#include <X11/cursorfont.h>
#include <limits>
#include <list>
#include <vector>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/extensions/Xinerama.h>
#include <X11/Xmu/WinUtil.h>

struct Size {
    unsigned width;
    unsigned height;
};
struct Geometry {
    Size size;
    int x;
    int y;
};

struct Range {
    long start;
    long end;
    bool empty() { return start == end; }
    bool aligned(long origin, long extent)
        { return !empty() && start <= origin + extent && end >= origin; }
};

struct X11Env;

struct PartialStrut {
    long left;
    long right;
    long top;
    long bottom;
    Range rleft;
    Range rright;
    Range rtop;
    Range rbottom;
    void box(const X11Env &x11, Geometry &g);
};

struct X11Env {
    Display *display;
    Window root;
    Geometry rootGeom;
    std::vector<Geometry> monitors;

    X11Env(Display *display_)
    : display(display_)
    , root(XDefaultRootWindow(display))
    , rootGeom(getGeometry(root))

    {
        detectMonitors();
    }

    Atom atom(const char *name) { return XInternAtom(display, name, False); }

    Atom NetActiveWindow = atom("_NET_ACTIVE_WINDOW");
    Atom AWindow = atom("WINDOW");
    Atom Cardinal = atom("CARDINAL");
    Atom VisualId = atom("VISUALID");
    Atom NetMoveResizeWindow = atom("_NET_MOVERESIZE_WINDOW");
    Atom NetFrameExtents = atom("_NET_FRAME_EXTENTS");
    Atom NetClientList = atom("_NET_CLIENT_LIST");
    Atom NetWmStrut = atom("_NET_WM_STRUT");
    Atom NetWmStrutPartial = atom("_NET_WM_STRUT_PARTIAL");
    Atom NetWmStateFullscreen = atom("_NET_WM_STATE_FULLSCREEN");
    Atom NetWmStateBelow = atom("_NET_WM_STATE_BELOW");
    Atom NetWmStateAbove = atom("_NET_WM_STATE_ABOVE");
    Atom NetWmState = atom("_NET_WM_STATE");
    Atom NetWmDesktop = atom("_NET_WM_DESKTOP");
    Atom NetWmStateAdd = atom("_NET_WM_STATE_ADD");
    Atom NetWmStateMaximizedVert = atom("_NET_WM_STATE_MAXIMIZED_VERT");
    Atom NetWmStateMaximizedHoriz = atom("_NET_WM_STATE_MAXIMIZED_HORZ");
    Atom NetWmStateShaded = atom("_NET_WM_STATE_SHADED");

    void detectMonitors(); // Get the geometry of the monitors.

    Geometry getGeometry(Window w) const {
        Window root;
        return getGeometry(w, &root);
    }

    Geometry getGeometry(Window w, Window *root) const {
        Geometry returnValue;
        unsigned int borderWidth;
        unsigned int depth;
        Status s = XGetGeometry(display, w, root,  &returnValue.x, &returnValue.y,
                    &returnValue.size.width, &returnValue.size.height, &borderWidth, &depth);
        if (!s)
            throw "Can't get window geometry";
        return returnValue;
    }

    Window pick(); // pick a window on the display using the mouse.
    Window active(); // find active window
    int monitorForWindow(Window); // find index of monitor on which a window lies.
};

static int intarg() { return atoi(optarg); } // XXX: use strtol and invoke usage()
static bool nodo = false;
static int border = 0;

std::ostream &
operator<<(std::ostream &os, const Geometry &m)
{
    return os << "{ w:" << m.size.width
                << ", h: " << m.size.height
                << ", x: " << m.x
                << ", y: " << m.y
                << " }";
}

static void
usage()
{
    std::clog
<< "usage:" << std::endl
<< "fling [ -p ] [ -s <screen> ] [-b <border>] \\" << std::endl
<< "        <left|right|top|bottom|topleft|bottomleft|topright|bottomright>" << std::endl
<< "    move to specified area of screen." << std::endl
<< std::endl
<< "fling [ -p ] [ -s screen ] \\" << std::endl
<< "       <numx>[/denomx[:spanx]] <numy>[/denomy[:spany]]" << std::endl
<< "    move left edge of window to (numx,numy) in a grid size " << std::endl
<< "    (denomx,denomy), spanning a rectangle of size spanx, spany gridpoints." << std::endl
<< std::endl
<< "fling -f [ -p ] [ -s screen ]" << std::endl
<< "    toggle fullscreen status of window " << std::endl
<< "fling -u [ -p ] [ -s screen ]" << std::endl
<< "    toggle window to be below all others (can combine with -f)" << std::endl
<< std::endl
<< " -p allows you to choose the target window for all invocations, otherwise," << std::endl
<< " the window is moved. For use in a terminal emulator, this means fling will" << std::endl
<< " fling your terminal around" << std::endl
;
    exit(1);
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
adjustForStruts(const X11Env &x11, Geometry *g, long targetDesktop)
{
    Atom actualType;
    int actualFormat;
    unsigned long itemCount;
    unsigned long afterBytes;
    unsigned char *prop;
    // get a list of all clients, so we can adjust monitor sizes for extents.
    int rc = XGetWindowProperty(x11.display, XDefaultRootWindow(x11.display), x11.NetClientList,
            0, std::numeric_limits<long>::max(), False, x11.AWindow,
            &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
    if (rc != 0 || actualFormat != 32 || itemCount <= 0 ) {
        std::cerr << "can't list clients to do strut processing" << std::endl;
        return;
    }

    Window *w = (Window *)prop;
    for (size_t i = itemCount; i-- > 0;) {
        // if the window is on the same desktop...
        rc = XGetWindowProperty(x11.display, w[i], x11.NetWmDesktop, 0,
                std::numeric_limits<long>::max(), False, x11.Cardinal,
                &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
        if (rc == 0) {
            long clipDesktop = *(long *)prop;
            XFree(prop);
            if (clipDesktop == targetDesktop || clipDesktop == -1 || targetDesktop == -1) {
                rc = XGetWindowProperty(x11.display, w[i], x11.NetWmStrutPartial,
                    0, std::numeric_limits<long>::max(), False, x11.Cardinal,
                    &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
                if (rc == 0) {
                    if (itemCount == 12 && actualFormat == 32) {
                        PartialStrut *strut = (PartialStrut *)prop;
                        strut->box(x11, *g);
                    }
                    XFree(prop);
                }
            }
        }
    }
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
getGeom(const char *val, int &numerator, unsigned &denominator, unsigned &extent)
{
    char *p;
    numerator = strtol(val, &p, 0);
    if (*p == '/') {
        denominator = strtol(p + 1, &p, 0);
        if (*p == ':')
            extent = strtol(p + 1, &p, 0);
        else
            extent = 1;
    } else {
        extent = numerator = denominator = 1;
    }
}

static void
toggleFlag(const X11Env &x11, Window win, const Atom toggle)
{
    XEvent e;
    XClientMessageEvent &ec = e.xclient;
    memset(&e, 0, sizeof e);
    ec.type = ClientMessage;
    ec.serial = 1;
    ec.send_event = True;
    ec.message_type = x11.NetWmState;
    ec.window = win;
    ec.format = 32;
    ec.data.l[0] = 2; //_NET_WM_STATE_TOGGLE;
    ec.data.l[1] = toggle;
    ec.data.l[2] = toggle == x11.NetWmStateMaximizedHoriz ?  x11.NetWmStateMaximizedVert : 0 ;
    ec.data.l[3] = 1;
    if (!XSendEvent(x11.display, x11.root, False, SubstructureRedirectMask|SubstructureNotifyMask, &e))
        std::cerr << "can't go fullscreen" << std::endl;
    XSync(x11.display, False);
}

static void
moveWindow(const X11Env &x11, Window win, const Geometry &geom)
{
    // Tell the WM where to put it.
    XEvent e;
    XClientMessageEvent &ec = e.xclient;
    ec.type = ClientMessage;
    ec.serial = 1;
    ec.send_event = True;
    ec.message_type = x11.NetMoveResizeWindow;
    ec.window = win;
    ec.format = 32;
    ec.data.l[0] = 0xf0a;
    ec.data.l[1] = geom.x;
    ec.data.l[2] = geom.y;
    ec.data.l[3] = geom.size.width;
    ec.data.l[4] = geom.size.height;
    if (!nodo) {
        XSendEvent(x11.display, x11.root, False, SubstructureRedirectMask|SubstructureNotifyMask, &e);
        XSync(x11.display, False);
    }
}

int
main(int argc, char *argv[])
{
    int screen = -1, c;
    bool doPick = false;

    std::list<Atom> toggles;

    Display *display = XOpenDisplay(0);
    if (display == 0) {
        std::clog << "failed to open display: set DISPLAY environment variable" << std::endl;
        return 1;
    }
    X11Env x11(display);

    while ((c = getopt(argc, argv, "ab:g:ns:fmpuh")) != -1) {
        switch (c) {
            case 'p':
                doPick = true;
                break;
            case 'b':
                border = intarg();
                break;
            case 's':
                screen = intarg();
                break;
            case 'n':
                nodo = true;
                break;
            case 'f':
                toggles.push_back(x11.NetWmStateFullscreen);
                break;
            case 'm':
                toggles.push_back(x11.NetWmStateMaximizedHoriz);
                break;
            case 'h':
                toggles.push_back(x11.NetWmStateShaded);
                break;
            case 'a':
                toggles.push_back(x11.NetWmStateAbove);
                break;
            case 'u':
                toggles.push_back(x11.NetWmStateBelow);
                break;
        }
    }

    // Which window are we modifying?
    Window win = doPick ? x11.pick() : x11.active();

    // If we're doing state toggles, do them now.
    for (auto atom : toggles)
        toggleFlag(x11, win, atom);

    // If nothing else to do, just exit.
    if (argc == optind)
        return 0;

    /*
     * get the extent of the frame around the window: we assume the new frame
     * will have the same extents when we resize it, and use that to adjust the
     * position of the client window so its frame abuts the edge of the screen.
     */
    Atom actualType;
    int actualFormat;
    unsigned long itemCount;
    unsigned long afterBytes;
    const long *frame;
    unsigned char *prop;
    long desktop;
    int rc = XGetWindowProperty(x11.display, win, x11.NetFrameExtents,
            0, std::numeric_limits<long>::max(), False, x11.Cardinal,
            &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
    if (rc != 0 || actualFormat != 32 || itemCount != 4) {
        std::cerr << "can't find frame sizes" << std::endl;
        static long defaultFrame[] = { 0, 0, 0, 0 };
        frame = defaultFrame;
    } else {
        frame = (long *)prop;
    }

    rc = XGetWindowProperty(x11.display, win, x11.NetWmDesktop, 0,
            std::numeric_limits<long>::max(), False, x11.Cardinal,
            &actualType, &actualFormat, &itemCount, &afterBytes, &prop);

    desktop = rc == 0 ? *(long *)prop : 0xffffffff;

    // now work out where to put the window.
    struct Grid {
        Size screen;
        Geometry window;
    };

    struct shortcut {
        const char *name;
        Grid data;
    };

    shortcut shortcuts[] = {
        { "top",        { { 1, 2 }, { { 1, 1 }, 0, 0 }} } ,
        { "bottom",     { { 1, 2 }, { { 1, 1 }, 0, 1 }} } ,
        { "left",       { { 2, 1 }, { { 1, 1 }, 0, 0 }} } ,
        { "right",      { { 2, 1 }, { { 1, 1 }, 1, 0 }} } ,
        { "topleft",    { { 2, 2 }, { { 1, 1 }, 0, 0 }} } ,
        { "topright",   { { 2, 2 }, { { 1, 1 }, 1, 0 }} } ,
        { "bottomleft", { { 2, 2 }, { { 1, 1 }, 0, 1 }} } ,
        { "bottomright",{ { 2, 2 }, { { 1, 1 }, 1, 1 }} } ,
        { 0 }
    };

    if (screen == -1)
        screen = x11.monitorForWindow(win);
    const Geometry &monitor = x11.monitors[screen];

    Grid *data = 0;
    Grid manual;
    Geometry window;

    if (argc - optind == 1) {
        for (shortcut *sc = shortcuts;; sc++) {
            if (sc->name == 0)
                break;
            if (strcmp(argv[optind], sc->name) == 0) {
                data = &sc->data;
                break;
            }
        }
    } else if (argc - optind == 2) {
        data = &manual;
        getGeom(argv[optind++], data->window.x, data->screen.width, data->window.size.width);
        getGeom(argv[optind++], data->window.y, data->screen.height, data->window.size.height);
        manual.window.x--;
        manual.window.y--;
    }

    if (data) {
        // original fractional spec.
        window.size.width = monitor.size.width * data->window.size.width / data->screen.width;
        window.size.height = monitor.size.height * data->window.size.height / data->screen.height;
        window.x = monitor.size.width * data->window.x / data->screen.width;
        window.y = monitor.size.height * data->window.y / data->screen.height;
    } else {
        // macro-style "up" "down" "left" "right"

        // start with monitor-sized window at monitor's origin.
        window.size = monitor.size;
        window.x = 0;
        window.y = 0;
        char c;
        for (const char *path = argv[optind]; (c = *path) != 0; ++path) {
            switch (c) {
                case '.':
                    break;
                case 'r':
                    // right move - move to right, and then...
                    window.x += window.size.width / 2;
                case 'l':
                    // left move - cut out right hand side.
                    window.size.width /= 2;
                    break;
                case 'd':
                    window.y += window.size.height / 2;
                case 'u':
                    window.size.height /= 2;
                    break;
                case 'h':
                    // reduce horizontal size and centre
                    window.size.width /= 2;
                    window.x += window.size.width / 2;
                    break;
                case 'v':
                    // reduce vertical size and centre
                    window.size.height /= 2;
                    window.y += window.size.height / 2;
                    break;
                default:
                    usage();
            }
        }
    }

    // monitor-relative -> root relative
    window.x += monitor.x;
    window.y += monitor.y;

    // make sure the window doesn't cover any struts.
    adjustForStruts(x11, &window, desktop);

    // Now have the geometry for the frame. Adjust to client window size,
    // assuming frame will remain the same.
    window.size.width -= frame[0] + frame[1] + border * 2;
    window.size.height -= frame[2] + frame[3] + border * 2;
    window.x += frame[0] + border;
    window.y += frame[2] + border;
    moveWindow(x11, win, window);
}
