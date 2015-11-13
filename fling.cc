#include "wmhack.h"
#include <X11/Xatom.h>
#include <string>
#include <map>

static int intarg() { return atoi(optarg); } // XXX: use strtol and invoke usage()
static bool nodo = false;
static int border = 0;

extern char readme_txt[];
static void
usage()
{
    std::clog << readme_txt;
    exit(1);
}

static void
adjustForStruts(const X11Env &x11, Geometry *g, long targetDesktop)
{
    Atom actualType;
    int actualFormat;
    unsigned long itemCount;
    unsigned long afterBytes;
    unsigned char *winlist;
    // get a list of all clients, so we can adjust monitor sizes for extents.
    int rc = XGetWindowProperty(x11, x11.root, x11.NetClientList,
            0, std::numeric_limits<long>::max(), False, x11.AWindow,
            &actualType, &actualFormat, &itemCount, &afterBytes, &winlist);
    if (rc != 0 || actualFormat != 32 || itemCount <= 0 ) {
        std::cerr << "can't list clients to do strut processing" << std::endl;
        return;
    }

    Window *w = (Window *)winlist;
    for (size_t i = itemCount; i-- > 0;) {
        auto clipDesktop = x11.desktopForWindow(w[i]);
        unsigned char *prop;
        if (clipDesktop == targetDesktop || clipDesktop == -1 || targetDesktop == -1) {
            rc = XGetWindowProperty(x11, w[i], x11.NetWmStrutPartial,
                0, std::numeric_limits<long>::max(), False, x11.Cardinal,
                &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
            if (rc == 0) {
                if (itemCount == 12 && actualFormat == 32) {
                    PartialStrut *strut = (PartialStrut *)prop;
                    strut->box(x11, *g);
                }
                XFree(prop);
            } else {
                unsigned char *prop;
                rc = XGetWindowProperty(x11, w[i], x11.NetWmStrut,
                    0, std::numeric_limits<long>::max(), False, x11.Cardinal,
                    &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
                if (rc == 0) {
                    std::clog << "TODO: deal with legacy strut\n";
                    XFree(prop);
                }
            }
        }
    }
    XFree(winlist);
}

static void
setOpacity(const X11Env &x11, Window w, double opacity)
{
    unsigned long opacityLong = opacity * std::numeric_limits<uint32_t>::max();
    XChangeProperty(x11, w, x11.NetWmOpacity, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&opacityLong, 1);
    XFlush(x11);
}

static void
setWorkdir(const X11Env &x11, Window w, const char *value)
{
    XChangeProperty(x11, w, x11.WorkDir, XA_STRING, 8, PropModeReplace,
               (const unsigned char *)value, strlen(value));
    XFlush(x11);
}

int
main(int argc, char *argv[])
{
    int screen = -1, c;
    int verbose = 0;
    bool doPick = false;
    double opacity = -1;
    bool windowRelative = false;
    Window win = 0;
    const char *workdir = 0;
    bool doGeom = false;

    std::list<Atom> toggles;

    Display *display = XOpenDisplay(0);
    if (display == 0) {
        std::clog << "failed to open display: set DISPLAY environment variable" << std::endl;
        return 1;
    }
    X11Env x11(display);

    if (argc == 1)
        usage();
    while ((c = getopt(argc, argv, "b:o:s:w:W:afghmnpuvx_")) != -1) {
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
            case 'g':
                doGeom = true;
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
            case 'W':
               workdir = optarg;
               break;
            case 'v':
               verbose++;
               break;
            default:
               usage();
               break;
        }
    }

    // Which window are we modifying?
    if (win == 0)
       win = doPick ? x11.pick() : x11.active();
    if (win == 0) {
        std::cerr << "no window selected\n";
        return 0;
    }

    // If we're doing state toggles, do them now.

    if (opacity >= 0.0) {
        setOpacity(x11, win, opacity);
        return 0;
    }
    if (workdir != 0) {
        setWorkdir(x11, win, workdir);
        return 0;
    }

    if (doGeom) {
       std::cout << "geometry: " << x11.getGeometry(win) << "\n";
       exit(0);
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
    unsigned char *prop;
    long desktop;
    int rc;

    if (screen == -1)
        screen = x11.monitorForWindow(win);

    const long *frame;
    rc = XGetWindowProperty(x11, win, x11.NetFrameExtents,
            0, std::numeric_limits<long>::max(), False, x11.Cardinal,
            &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
    bool haveFrame = rc == 0;
    if (rc != 0 || actualFormat != 32 || itemCount != 4) {
        std::cerr << "can't find frame sizes" << std::endl;
        static long defaultFrame[] = { 0, 0, 0, 0 };
        frame = defaultFrame;
    } else {
        frame = (long *)prop;
    }

    const Geometry *monitorGeometry;
    Geometry window;
    if (windowRelative) {
       monitorGeometry = 0;
       window = x11.getGeometry(win);
       window.size.width += frame[0] + frame[1];
       window.size.height += frame[2] + frame[3];
       window.x -= frame[0];
       window.y -= frame[2];
       if (rc == 0)
           XFree(prop);
    } else {
       monitorGeometry = &x11.monitors[screen];
       window = *monitorGeometry;
    }

    /*
     * Find desktop of the window in question - we ignore windows on other
     * desktops for struts avoidance, etc.
     */
    desktop = x11.desktopForWindow(win);

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

    // Adjust for the WM's frame around the window,
    // assuming frame will remain the same after the move.
    window.size.width -= frame[0] + frame[1] + border * 2;
    window.size.height -= frame[2] + frame[3] + border * 2;
    window.x += frame[0] + border;
    window.y += frame[2] + border;
    if (haveFrame)
        XFree((unsigned char *)frame);
    Geometry oldwindow = x11.getGeometry(win);
    x11.updateState(win, x11.NetWmStateShaded, X11Env::REMOVE);
    x11.updateState(win, x11.NetWmStateMaximizedHoriz, X11Env::REMOVE);
    x11.updateState(win, x11.NetWmStateFullscreen, X11Env::REMOVE);

    int duration = 200000;
    int sleeptime = 1000000 / 60;

    int iters = duration / sleeptime;
#define update(f) next.f = (oldwindow.f * (iters - i) + window.f * i) / iters
    for (auto i = 1;; ++i) {
          Geometry next;
          update(size.width);
          update(size.height);
          update(x);
          update(y);
          x11.setGeometry(win, next);
          if (verbose) {
             std::cout << "set geometry on " << win << " to " << next;
          }
          if (i == iters)
             break;
          usleep(sleeptime);
    }
    XCloseDisplay(display);
}
