#include "wmhack.h"
#include <X11/Xatom.h>
#include <string>
#include <map>

static int intarg() { return atoi(optarg); } // XXX: use strtol and invoke usage()
static bool nodo = false;
static int border = 0;

static void
usage()
{
    std::clog
<< "usage:" << std::endl
<< "fling [ -x ] [ -p | -w  <windowid> ] [ -s <screen> ] [-b <border>] \\" << std::endl
<< "        <left|right|top|bottom|topleft|bottomleft|topright|bottomright>" << std::endl
<< "    move to specified area of screen." << std::endl
<< "fling [ -w ] [ -p  | -w <windowid> ] [ -s screen ] <[percent]<u|d|l|r|v|h>+" << std::endl
<< "    set new area to (top|bottom|left|right|vertical middle|horizontal" << std::endl
<< "    middle) of current area. Current area starts at full screen, and window" << std::endl
<< "    is moved to current area at end of string. For example, 'fling 60lu'" << std::endl
<< "    moves a window to the leftmost 60% of the top half of the screen"
<< std::endl
<< "fling -f [ -p | -w <windowid> ] [ -s screen ]" << std::endl
<< "    toggle fullscreen status of window " << std::endl
<< "fling -u [ -p | -w <windowid> ] [ -s screen ]" << std::endl
<< "    toggle window to be below all others (can combine with -f)" << std::endl
<< "fling -o <opacity> [ -p | -w <windowid> ] [ -s screen ]" << std::endl
<< "    set window opacity (0.0 to 1.0)" << std::endl
<< std::endl
<< " -p allows you to choose the target window for all invocations, otherwise," << std::endl
<< " the window is moved. For use in a terminal emulator, this means fling will" << std::endl
<< " fling your terminal around" << std::endl
<< " -w allows you to specify a window id that you've gotten from elsewhere "
<< "(eg, $WINDOWID in xterm)" << std::endl
<< " -x indicates that changes are relative to the existing window dimensions, " << std::endl
<< " rather than the entire screen" << std::endl
;
    exit(1);
}

static void
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
        if (rc == 0 && itemCount != 0) {
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

static void
setOpacity(const X11Env &x11, Window w, double opacity)
{
    unsigned long opacityLong = opacity * std::numeric_limits<uint32_t>::max();
    XChangeProperty(x11.display, w, x11.NetWmOpacity, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&opacityLong, 1);
    XFlush(x11.display);
}

int
main(int argc, char *argv[])
{
    int screen = -1, c;
    bool doPick = false;
    double opacity = -1;
    bool windowRelative = false;
    Window win = 0;

    std::list<Atom> toggles;

    Display *display = XOpenDisplay(0);
    if (display == 0) {
        std::clog << "failed to open display: set DISPLAY environment variable" << std::endl;
        return 1;
    }
    X11Env x11(display);

    if (argc == 1)
        usage();
    while ((c = getopt(argc, argv, "b:g:o:s:w:afhmnpux_")) != -1) {
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
                usage();
                break;
            case '_':
                toggles.push_back(x11.NetWmStateShaded);
                break;
            case 'a':
                toggles.push_back(x11.NetWmStateAbove);
                break;
            case 'u':
                toggles.push_back(x11.NetWmStateBelow);
                break;
            case 'o':
               opacity = strtod(optarg, 0);
               if (opacity < 0.0 || opacity > 1.0)
                  usage();
               break;
            case 'w':
               win = intarg();
               break;
            case 'x':
               windowRelative = true;
               border = 0; // assume the window already has adequate space around it.
               break;
        }
    }

    // Which window are we modifying?
    if (win == 0)
       win = doPick ? x11.pick() : x11.active();

    // If we're doing state toggles, do them now.

    if (opacity >= 0.0) {
      setOpacity(x11, win, opacity);
      return 0;
    }

    for (auto atom : toggles)
        x11.updateState(win, atom, X11Env::TOGGLE);

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

    /*
     * Find desktop of the window in question - we ignore windows on other
     * desktops for struts avoidance, etc.
     */
    rc = XGetWindowProperty(x11.display, win, x11.NetWmDesktop, 0,
            std::numeric_limits<long>::max(), False, x11.Cardinal,
            &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
    desktop = rc == 0 ? *(long *)prop : 0xffffffff;

    static std::map<std::string, const char *> aliases = {
        { "top",        "u" },
        { "bottom",     "d" },
        { "left",       "l" },
        { "right",      "r" },
        { "topleft",    "ul" },
        { "topright",   "ur" },
        { "bottomleft", "dl" },
        { "bottomright", "dr" },
    };

    if (screen == -1)
        screen = x11.monitorForWindow(win);
    const Geometry *monitorGeometry;
    Geometry window;
    if (windowRelative) {
       monitorGeometry = 0;
       window = x11.getGeometry(win);
       window.size.width += frame[0] + frame[1];
       window.size.height += frame[2] + frame[3];
       window.x -= frame[0];
       window.y -= frame[2];
    } else {
       monitorGeometry = &x11.monitors[screen];
       window = *monitorGeometry;
    }
    
    const char *location = argv[optind];
    auto alias = aliases.find(location);
    if (alias != aliases.end())
        location = alias->second;

    // start with monitor-sized window at monitor's origin.
    char curChar;
    for (const char *path = location; (curChar = *path) != 0; ++path) {
        int scale;
        if (isdigit(curChar)) {
            char *newpath;
            scale = strtol(path, &newpath, 10);
            path = newpath;
            curChar = *path;
            if (scale >= 100 || scale <= 0)
                usage();
        } else {
            scale = 50;
        }
        switch (curChar) {
            case '.':
                break;
            case 'r':
                // move to right
                window.x += window.size.width * (100 - scale) / 100;
                // and then...
            case 'l':
                // cut out right hand side.
                window.size.width = window.size.width * scale / 100;
                break;
            case 'd':
                window.y += window.size.height * (100 - scale) / 100;
                // and then...
            case 'u':
                window.size.height = window.size.height * scale / 100;
                break;
            case 'h': {
                // reduce horizontal size and centre
                int newsize = window.size.width * scale / 100;
                window.x += (window.size.width - newsize) / 2;
                window.size.width = newsize;
                break;
            }
            case 'v': {
                // reduce vertical size and centre
                int newsize = window.size.height * scale / 100;
                window.y += (window.size.height - newsize) / 2;
                window.size.height = newsize;
                break;
            }
            default:
                usage();
        }
    }

    // make sure the window doesn't cover any struts.
    adjustForStruts(x11, &window, desktop);

    // Now have the geometry for the frame. Adjust to client window size,
    // assuming frame will remain the same.
    window.size.width -= frame[0] + frame[1] + border * 2;
    window.size.height -= frame[2] + frame[3] + border * 2;
    window.x += frame[0] + border;
    window.y += frame[2] + border;
    x11.updateState(win, x11.NetWmStateShaded, X11Env::REMOVE);
    x11.updateState(win, x11.NetWmStateMaximizedHoriz, X11Env::REMOVE);
    x11.setGeometry(win, window);
}
