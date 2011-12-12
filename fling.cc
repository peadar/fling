#include <iostream>
#include <limits>
#include <vector>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/extensions/Xinerama.h>

struct Geometry {
    unsigned width;
    unsigned height;
    int x;
    int y;
};
enum Gravity { Low, Center, High };

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
    {}
};

static std::vector<Geometry> monitors;
static Geometry rootGeom;
static int verbose;
static int intarg() { return atoi(optarg); } // XXX: use strtol and invoke usage()
static bool nodo = false;

std::ostream &
operator<<(std::ostream &os, const Geometry &m)
{
    return os << "{ w:" << m.width
                << ", h: " << m.height
                << ", x: " << m.x
                << ", y: " << m.y
                << " }";
}

static void
usage()
{
    std::clog
<< "usage: fling [ -v ] [ -s screen ] ( left | right | top | bottom | topleft | bottomleft | topright | bottomright" << std::endl
<< "     : fling [ -v ] [ -s screen ] <percentage>(n|s|c) <percentage>(e|w|c)" << std::endl
<< " -v adds verbose output" << std::endl
<< " The percentile version indicates what percentage of the display to take up" << std::endl
<< " on the vertical and horizontal axes. The n, s, e, w, and c suffixes to the" << std::endl
<< " percentages indicate the gravity, i.e., the screen edge you wish to attach" << std::endl
<< " to the window." << std::endl
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
        std::cout << "Can't get geometry: " << s << std::endl;
        return 0;
    }
    s = XTranslateCoordinates(x11, win, winroot,  x, y, &x, &y, &winroot);
    if (!s) {
        std::cout << "Can't translate window coordinates" << s << std::endl;
        return 0;
    }
    int midX = x + w / 2;
    int midY = y + h / 2;

    // XXX: really need to sort by area of window on the monitor.
    int num = 0;
    for (int i = 0; i < monitors.size(); ++i) {
        Geometry &mon = monitors[i];
        if (midX >= mon.x && midX < mon.x + mon.width
                && midY >= monitors[i].y && midY < mon.y + mon.height) {
            std::clog << "window is on monitor " << i << "; " << mon << std::endl;
            return i;
        }
    }
    return 0;
}

void
PartialStrut::box(Geometry &g)
{
    long winend, strutend;
    if (rtop.aligned(g.x, g.width) && top > g.y) {
        g.height += top - g.y;
        g.y = top;
    }
    if (rleft.aligned(g.y, g.height) && left > g.x) {
        g.width += left - g.x;
        g.x = left;
    }
    winend = g.y + g.height;
    strutend = rootGeom.height - bottom;
    if (rbottom.aligned(g.x, g.width) && strutend < winend)
        g.height += strutend - winend; 
    winend = g.x + g.width;
    strutend = rootGeom.width - right;
    if (rright.aligned(g.y, g.height) && strutend < winend)
        g.width += strutend - winend; 
}

static void
detectMonitors(Display *x11, const Atoms &a)
{
    Atom actualType;
    int actualFormat;
    unsigned long itemCount;
    unsigned long afterBytes;
    unsigned char *prop;
    // If xinerama is present, use it.
    int eventBase, eventError;
    if (XineramaQueryExtension(x11, &eventBase, &eventError) == 0) {
        monitors.resize(1);
        monitors[0] = rootGeom;
        if (verbose)
            std::cout << "no xinerama" << std::endl;
    } else {
        int monitorCount;
        XineramaScreenInfo *xineramaMonitors = XineramaQueryScreens(x11, &monitorCount);
        if (xineramaMonitors == 0) {
            std::cerr << "have xinerama, but can't get monitor info" << std::endl;
            exit(1);
        }
        monitors.resize(monitorCount);
        for (size_t i = 0; i < monitorCount; ++i) {
            monitors[i].width = xineramaMonitors[i].width;
            monitors[i].height = xineramaMonitors[i].height;
            monitors[i].x = xineramaMonitors[i].x_org;
            monitors[i].y = xineramaMonitors[i].y_org;
        }
        XFree(xineramaMonitors);
        if (verbose)
            std::cout << "found xinerama" << std::endl;
    }
    if (verbose) 
        for (std::vector<Geometry>::const_iterator i = monitors.begin(); i != monitors.end(); ++i)
            std::cout << "monitor: " << *i << "\n";
}

void
adjustForStruts(Display *x11, Geometry *g, const Atoms &a)
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
        if (verbose)
            std::cout << "can't find clients to do strut processing";
    } else {
        Window *w = (Window *)prop;
        for (size_t i = itemCount; i-- > 0;) {
            rc = XGetWindowProperty(x11, w[i], a.NetWmStrutPartial,
                0, std::numeric_limits<long>::max(), False, a.Cardinal, 
                &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
            if (rc == 0) {
                if (itemCount == 12 && actualFormat == 32) {
                    PartialStrut *strut = (PartialStrut *)prop;
                    std::cout << "window " << w[i] << " has partial struts" << std::endl;
                    strut->box(*g);
                }
                XFree(prop);
            }
        }
    }
}

static
void getWeight(char *str, long *mag, Gravity *grav, const char *names)
{
    char *p;
    *mag = strtol(str, &p, 10);
    if (*p == names[0])
        *grav = Low;
    else if (*p == names[1])
        *grav = Center;
    else if (*p == names[2])
        *grav = High;
    else
        usage();
}

int
main(int argc, char *argv[])
{
    int screen = -1, c;
    const char *gridName = "2x2";

    while ((c = getopt(argc, argv, "vg:ns:")) != -1) {
        switch (c) {
            case 'v':
                verbose++;
                break;
            case 'g':
                gridName = optarg;
                break;

            case 's':
                screen = intarg();
                break;
            case 'n':
                nodo = true;
                break;

        }
    }

    // Connect to display
    Display *x11 = XOpenDisplay(0);
    Atoms a(x11);
    Window root = XDefaultRootWindow(x11);

    // Get geometry of root window.
    Window tmp;
    unsigned bw, bd;
    XGetGeometry(x11, root, &tmp,  &rootGeom.x, &rootGeom.y, &rootGeom.width, &rootGeom.height, &bw, &bd);
    if (verbose)
        std::cout << "root geometry: " << rootGeom;

    // Get the geometry of the monitors.
    detectMonitors(x11, a);

    // Find active window from WM.
    Atom actualType;
    int actualFormat;
    unsigned long itemCount;
    unsigned long afterBytes;
    unsigned char *prop;
    int rc = XGetWindowProperty(x11, root, a.NetActiveWindow,
            0, std::numeric_limits<long>::max(), False, a.Window, 
            &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
    if (actualFormat == None || itemCount != 1) {
        std::cerr << "can't find active window";
        exit(1);
    }
    Window win = *(Window *)prop;

    /*
     * get the extent of the frame around the window: we assume the new frame
     * will have the same extents when we resize it, and use that to adjust the
     * position of the client window so its frame abuts the edge of the screen.
     */
    long *frame;
    rc = XGetWindowProperty(x11, win, a.NetFrameExtents,
            0, std::numeric_limits<long>::max(), False, a.Cardinal, 
            &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
    if (rc != 0 || actualFormat != 32 || itemCount != 4) {
        std::cerr << "can't find frame sizes";
    } else {
        frame =  (long *)prop;
    }
    if (verbose) {
        std::cout << "frame extents: " << "\t";
        for (size_t i = 0; i < itemCount; ++i)
            std::cout << frame[i] << (i + 1 == itemCount ? "\n" : ",");
    }

    // now work out where to put the window.
    long vert; // how much vertical space.
    long horiz; // how much horizontal space.
    Gravity vertGravity;
    Gravity horizGravity;
    struct shortcut {
        const char *name;
        Gravity vg;
        long vp;

        Gravity hg;
        long hp;
    };
    shortcut shortcuts[] = {
        { "top", Low, 50, Center, 100 },
        { "bottom", High, 50, Center, 100 },
        { "left", Center, 100, Low, 50 },
        { "right", Center, 100, High, 50 },
        { "topleft", Low, 50, Low, 50 },
        { "topright", Low, 50, High, 50 },
        { "bottomleft", High, 50, Low, 50 },
        { "bottomright", High, 50, High, 50 },
        { 0, Low, 100, High, 100 }
    };

    if (argc - optind == 1) {
        for (shortcut *sc = shortcuts;; sc++)
            if (sc->name == 0 || strcmp(argv[optind], sc->name) == 0) {
                vertGravity = sc->vg;
                horizGravity = sc->hg;
                vert = sc->vp;
                horiz = sc->hp;
                break;
            }
    } else if (argc - optind == 2) {
        getWeight(argv[optind], &vert, &vertGravity, "ncs");
        getWeight(argv[optind + 1], &horiz, &horizGravity, "wce");
    } else {
        usage();
    }

    if (screen == -1)
        screen = getMonitor(x11, win);

    Geometry &m = monitors[screen];
    Geometry w;

    w.width = m.width * horiz / 100;
    w.height = m.height * vert / 100;

    switch (horizGravity) {
        case Low: w.x = 0; break;
        case High: w.x = m.width - w.width; break;
        case Center: w.x = (m.width - w.width) / 2; break;
    }

    switch (vertGravity) {
        case Low: w.y = 0; break;
        case High: w.y = m.height - w.height; break;
        case Center: w.y = (m.height - w.height) / 2; break;
    }

    // monitor-relative -> root relative
    w.x += m.x;
    w.y += m.y;

    // make sure the window doesn't cover any struts.
    adjustForStruts(x11, &w, a);

    // Now have the geometry for the frame. Adjust to client window size,
    // assuming frame will remain the same.
    w.width -= frame[0] + frame[1];
    w.height -= frame[2] + frame[3];
    w.x += frame[0];
    w.y += frame[2];

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
    ec.data.l[1] = w.x;
    ec.data.l[2] = w.y;
    ec.data.l[3] = w.width;
    ec.data.l[4] = w.height;

    if (verbose) 
        std::cout << "new geometry: (" << ec.data.l[1] << ","
            << ec.data.l[2] << "), " << ec.data.l[3] << "x" << ec.data.l[4] << std::endl;

    if (!nodo) {
        XSendEvent(x11, root, False, SubstructureRedirectMask|SubstructureNotifyMask, &e);
        XSync(x11, False);
    }
}
