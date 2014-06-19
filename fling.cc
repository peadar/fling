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

struct PartialStrut {
    long left;
    long right;
    long top;
    long bottom;
    Range rleft;
    Range rright;
    Range rtop;
    Range rbottom;
    void box(Geometry &g);
};

struct Atoms {
    Atom NetActiveWindow;
    Atom Window;
    Atom Cardinal;
    Atom VisualId;
    Atom NetMoveResizeWindow;
    Atom NetFrameExtents;
    Atom NetClientList;
    Atom NetWmStrut;
    Atom NetWmStrutPartial;
    Atom NetWmStateFullscreen;
    Atom NetWmStateBelow;
    Atom NetWmStateAbove;
    Atom NetWmState;
    Atom NetWmDesktop;
    Atom NetWmStateAdd;
    Atom NetWmStateMaximizedVert;
    Atom NetWmStateMaximizedHoriz;
    Atom NetWmStateShaded;
    Atoms(Display *x11)
    : NetActiveWindow(XInternAtom(x11, "_NET_ACTIVE_WINDOW", False))
    , Window(XInternAtom(x11, "WINDOW", False))
    , Cardinal(XInternAtom(x11, "CARDINAL", False))
    , VisualId(XInternAtom(x11, "VISUALID", False))
    , NetMoveResizeWindow(XInternAtom(x11, "_NET_MOVERESIZE_WINDOW", False))
    , NetFrameExtents(XInternAtom(x11, "_NET_FRAME_EXTENTS", False))
    , NetClientList(XInternAtom(x11, "_NET_CLIENT_LIST", False))
    , NetWmStrut(XInternAtom(x11, "_NET_WM_STRUT", False))
    , NetWmStrutPartial(XInternAtom(x11, "_NET_WM_STRUT_PARTIAL", False))
    , NetWmStateFullscreen(XInternAtom(x11, "_NET_WM_STATE_FULLSCREEN", False))
    , NetWmStateBelow(XInternAtom(x11, "_NET_WM_STATE_BELOW", False))
    , NetWmStateAbove(XInternAtom(x11, "_NET_WM_STATE_ABOVE", False))
    , NetWmState(XInternAtom(x11, "_NET_WM_STATE", False))
    , NetWmDesktop(XInternAtom(x11, "_NET_WM_DESKTOP", False))
    , NetWmStateAdd(XInternAtom(x11, "_NET_WM_STATE_ADD", False))
    , NetWmStateMaximizedVert(XInternAtom(x11,  "_NET_WM_STATE_MAXIMIZED_VERT", False))
    , NetWmStateMaximizedHoriz(XInternAtom(x11, "_NET_WM_STATE_MAXIMIZED_HORZ", False))
    , NetWmStateShaded(XInternAtom(x11, "_NET_WM_STATE_SHADED", False))
    {}
};

static std::vector<Geometry> monitors;
static Geometry rootGeom;
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

static int
getMonitor(Display *x11, Window win)
{
    Window winroot;
    Status s;
    int x = 0, y = 0;
    unsigned int w = 0, h = 0, bw, bd;
    s = XGetGeometry(x11, win, &winroot,  &x, &y, &w, &h, &bw, &bd);
    if (!s) {
        std::cerr << "Can't get root window geometry" << std::endl;
        return 0;
    }
    s = XTranslateCoordinates(x11, win, winroot,  x, y, &x, &y, &winroot);
    if (!s) {
        std::cerr << "Can't translate root window coordinates" << std::endl;
        return 0;
    }
    int midX = x + w / 2;
    int midY = y + h / 2;

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
PartialStrut::box(Geometry &g)
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
    long strutend = rootGeom.size.height - bottom;
    if (rbottom.aligned(g.x, g.size.width) && strutend < winend)
        g.size.height -= winend - strutend;
    winend = g.x + g.size.width;
    strutend = rootGeom.size.width - right;
    if (rright.aligned(g.y, g.size.height) && strutend < winend)
        g.size.width -= winend - strutend;
}

static void
detectMonitors(Display *x11, const Atoms &a)
{
    // If xinerama is present, use it.
    int eventBase, eventError;
    if (XineramaQueryExtension(x11, &eventBase, &eventError) == 0) {
        monitors.resize(1);
        monitors[0] = rootGeom;
    } else {
        int monitorCount;
        XineramaScreenInfo *xineramaMonitors = XineramaQueryScreens(x11, &monitorCount);
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
adjustForStruts(Display *x11, Geometry *g, const Atoms &a, long targetDesktop)
{
    Atom actualType;
    int actualFormat;
    unsigned long itemCount;
    unsigned long afterBytes;
    unsigned char *prop;
    // get a list of all clients, so we can adjust monitor sizes for extents.
    int rc = XGetWindowProperty(x11, XDefaultRootWindow(x11), a.NetClientList,
            0, std::numeric_limits<long>::max(), False, a.Window,
            &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
    if (rc != 0 || actualFormat != 32 || itemCount <= 0 ) {
        std::cerr << "can't list clients to do strut processing" << std::endl;
        return;
    }

    Window *w = (Window *)prop;
    for (size_t i = itemCount; i-- > 0;) {
        // if the window is on the same desktop...
        rc = XGetWindowProperty(x11, w[i], a.NetWmDesktop, 0,
                std::numeric_limits<long>::max(), False, a.Cardinal,
                &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
        if (rc == 0) {
            long clipDesktop = *(long *)prop;
            XFree(prop);
            if (clipDesktop == targetDesktop || clipDesktop == -1 || targetDesktop == -1) {
                rc = XGetWindowProperty(x11, w[i], a.NetWmStrutPartial,
                    0, std::numeric_limits<long>::max(), False, a.Cardinal,
                    &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
                if (rc == 0) {
                    if (itemCount == 12 && actualFormat == 32) {
                        PartialStrut *strut = (PartialStrut *)prop;
                        strut->box(*g);
                    }
                    XFree(prop);
                }
            }
        }
    }
}

static Window
pick(Display *x11, Window root)
{
    Window w = root;
    Cursor c = XCreateFontCursor(x11, XC_question_arrow);

    if (XGrabPointer(x11, root, False, ButtonPressMask|ButtonReleaseMask,
            GrabModeSync, GrabModeAsync, None, c, CurrentTime) != GrabSuccess) {
        throw "can't grab pointer";
    }

    for (bool done = false; !done;) {
        XEvent event;
        XAllowEvents(x11, SyncPointer, CurrentTime);
        XWindowEvent(x11, root, ButtonPressMask|ButtonReleaseMask, &event);
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
    XUngrabPointer(x11, CurrentTime);
    XFreeCursor(x11, c);
    return XmuClientWindow(x11, w);
}

static Window
active(Display *x11, Window root, const Atoms &a)
{
    // Find active window from WM.
    Atom actualType;
    int actualFormat;
    unsigned long itemCount;
    unsigned long afterBytes;
    unsigned char *prop;
    int rc = XGetWindowProperty(x11, root, a.NetActiveWindow,
            0, std::numeric_limits<long>::max(), False, a.Window,
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
toggleFlag(Display *x11, Window root, Window win, const Atoms a, const Atom toggle)
{
    XEvent e;
    XClientMessageEvent &ec = e.xclient;
    memset(&e, 0, sizeof e);
    ec.type = ClientMessage;
    ec.serial = 1;
    ec.send_event = True;
    ec.message_type = a.NetWmState;
    ec.window = win;
    ec.format = 32;
    ec.data.l[0] = 2; //_NET_WM_STATE_TOGGLE;
    ec.data.l[1] = toggle;
    ec.data.l[2] = toggle == a.NetWmStateMaximizedHoriz ?  a.NetWmStateMaximizedVert : 0 ;
    ec.data.l[3] = 1;
    if (!XSendEvent(x11, root, False, SubstructureRedirectMask|SubstructureNotifyMask, &e))
        std::cerr << "can't go fullscreen" << std::endl;
    XSync(x11, False);
}

int
main(int argc, char *argv[])
{
    int screen = -1, c;
    bool doPick = false;

    std::list<Atom> toggles;

    Display *x11 = XOpenDisplay(0);
    if (x11 == 0) {
        std::clog << "failed to open display: set DISPLAY environment variable" << std::endl;
        return 1;
    }
    Atoms a(x11);
    Window root = XDefaultRootWindow(x11);

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
                toggles.push_back(a.NetWmStateFullscreen);
                break;
            case 'm':
                toggles.push_back(a.NetWmStateMaximizedHoriz);
                break;
            case 'h':
                toggles.push_back(a.NetWmStateShaded);
                break;
            case 'a':
                toggles.push_back(a.NetWmStateAbove);
                break;
            case 'u':
                toggles.push_back(a.NetWmStateBelow);
                break;
        }
    }

    // Which window are we modifying?
    Window win = doPick ? pick(x11, root) : active(x11, root, a);

    // If we're doing state toggles, do them now.
    if (toggles.size() > 0) {
        for (auto atom : toggles)
            toggleFlag(x11, root, win, a, atom);
    }
    if (argc == optind)
        return 0;

    // Get the geometry of the monitors.
    detectMonitors(x11, a);

    // Get geometry of root window.
    Window tmp;
    unsigned bw, bd;
    XGetGeometry(x11, root, &tmp,  &rootGeom.x, &rootGeom.y, &rootGeom.size.width, &rootGeom.size.height, &bw, &bd);



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
    int rc = XGetWindowProperty(x11, win, a.NetFrameExtents,
            0, std::numeric_limits<long>::max(), False, a.Cardinal,
            &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
    if (rc != 0 || actualFormat != 32 || itemCount != 4) {
        std::cerr << "can't find frame sizes" << std::endl;
        static long defaultFrame[] = { 0, 0, 0, 0 };
        frame = defaultFrame;
    } else {
        frame = (long *)prop;
    }

    rc = XGetWindowProperty(x11, win, a.NetWmDesktop, 0,
            std::numeric_limits<long>::max(), False, a.Cardinal,
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
        screen = getMonitor(x11, win);
    Geometry &monitor = monitors[screen];

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
        window = monitor;
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
    adjustForStruts(x11, &window, a, desktop);

    // Now have the geometry for the frame. Adjust to client window size,
    // assuming frame will remain the same.
    window.size.width -= frame[0] + frame[1] + border * 2;
    window.size.height -= frame[2] + frame[3] + border * 2;
    window.x += frame[0] + border;
    window.y += frame[2] + border;

    // Tell the WM where to put it.
    XEvent e;
    XClientMessageEvent &ec = e.xclient;
    ec.type = ClientMessage;
    ec.serial = 1;
    ec.send_event = True;
    ec.message_type = a.NetMoveResizeWindow;
    ec.window = win;
    ec.format = 32;
    ec.data.l[0] = 0xf0a;
    ec.data.l[1] = window.x;
    ec.data.l[2] = window.y;
    ec.data.l[3] = window.size.width;
    ec.data.l[4] = window.size.height;

    if (!nodo) {
        XSendEvent(x11, root, False, SubstructureRedirectMask|SubstructureNotifyMask, &e);
        XSync(x11, False);
    }
}
