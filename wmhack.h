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

struct X11Env;

struct Size {
    unsigned width;
    unsigned height;
};

struct Geometry {
    Size size;
    int x;
    int y;
};
extern std::ostream & operator<<(std::ostream &os, const Geometry &m);

struct Range {
    long start;
    long end;
    bool empty() { return start == end; }
    bool aligned(long origin, long extent)
        { return !empty() && start < origin + extent && end >= origin; }
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
    void box(const X11Env &x11, Geometry &g);
};

struct X11Env {
    Display *display;
    Window root;
    Geometry rootGeom;
    std::vector<Geometry> monitors;

    X11Env(Display *display_);
    Atom atom(const char *name) { return XInternAtom(display, name, False); }
    Atom NetCurrentDesktop = atom("_NET_CURRENT_DESKTOP");
    Atom NetActiveWindow = atom("_NET_ACTIVE_WINDOW");
    Atom NetDesktopNames = atom("_NET_DESKTOP_NAMES");
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
    Atom NetWmOpacity = atom("_NET_WM_WINDOW_OPACITY");
    Atom WorkDir = atom("_PME_WORKDIR");

    void detectMonitors(); // Get the geometry of the monitors.

    Geometry getGeometry(Window w) const;
    Geometry getGeometry(Window w, Window *root) const;
    void setGeometry(Window win, const Geometry &geom) const;
    Window pick(); // pick a window on the display using the mouse.
    Window active(); // find active window
    enum StateUpdateAction { REMOVE = 0, ADD = 1, TOGGLE = 2 };
    void updateState(Window win, const Atom toggle, StateUpdateAction update) const;
    int monitorForWindow(Window); // find index of monitor on which a window lies.
};
