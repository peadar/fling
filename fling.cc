#include "wmhack.h"
#include <X11/Xatom.h>
#include <X11/keysymdef.h>
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
    unsigned char *prop;
    // get a list of all clients, so we can adjust monitor sizes for extents.
    int rc = XGetWindowProperty(x11, x11.root, x11.NetClientList,
            0, std::numeric_limits<long>::max(), False, x11.AWindow,
            &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
    if (rc != 0 || actualFormat != 32 || itemCount <= 0 ) {
        std::cerr << "can't list clients to do strut processing" << std::endl;
        return;
    }

    Window *w = (Window *)prop;
    for (size_t i = itemCount; i-- > 0;) {
        // if the window is on the same desktop...
        rc = XGetWindowProperty(x11, w[i], x11.NetWmDesktop, 0,
                std::numeric_limits<long>::max(), False, x11.Cardinal,
                &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
        if (rc == 0 && itemCount != 0) {
            long clipDesktop = *(long *)prop;
            XFree(prop);
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
                    rc = XGetWindowProperty(x11, w[i], x11.NetWmStrut,
                        0, std::numeric_limits<long>::max(), False, x11.Cardinal,
                        &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
                    if (rc == 0) {
                        std::clog << "TODO: deal with legacy strut\n";
                    }
                 }
            }
        }
    }
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

void
resizeWindow(X11Env &x11, long desktop, Geometry &geom, Window win, const long *frame, const char *location)
{
    char curChar;


    // Increase geometry by the size of the frame to include it in ratios
    geom.size.width += frame[0] + frame[1];
    geom.size.height += frame[2] + frame[3];
    geom.x -= frame[0];
    geom.y -= frame[2];

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
                geom.x += geom.size.width * (100 - scale) / 100;
                // and then...
            case 'l':
                // cut out right hand side.
                geom.size.width = geom.size.width * scale / 100;
                break;
            case 'd':
                geom.y += geom.size.height * (100 - scale) / 100;
                // and then...
            case 'u':
                geom.size.height = geom.size.height * scale / 100;
                break;
            case 'h': {
                // reduce horizontal size and centre
                int newsize = geom.size.width * scale / 100;
                geom.x += (geom.size.width - newsize) / 2;
                geom.size.width = newsize;
                break;
            }
            case 'v': {
                // reduce vertical size and centre
                int newsize = geom.size.height * scale / 100;
                geom.y += (geom.size.height - newsize) / 2;
                geom.size.height = newsize;
                break;
            }
            default:
                usage();
        }
    }

    // make sure the window doesn't cover any struts.
    adjustForStruts(x11, &geom, desktop);

    // Readjust to remove the size of the frame.
    geom.size.width -= frame[0] + frame[1] + border * 2;
    geom.size.height -= frame[2] + frame[3] + border * 2;
    geom.x += frame[0] + border;
    geom.y += frame[2] + border;


    Geometry oldgeom = x11.getGeometry(win);
    int duration = 200000;
    int sleeptime = 1000000 / 60;
    int iters = duration / sleeptime;
#define update(f) next.f = (oldgeom.f * (iters - i) + geom.f * i) / iters
    for (auto i = 1;; ++i) {
          Geometry next;
          update(size.width);
          update(size.height);
          update(x);
          update(y);
          x11.setGeometry(win, next);
          if (i == iters)
             break;
          usleep(sleeptime);
    }
#undef update
}

int
main(int argc, char *argv[])
{
    int screen = -1, c;
    int verbose = 0;
    bool doPick = false;
    bool interactive = false;
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
    while ((c = getopt(argc, argv, "b:o:s:w:W:afghimnpuvx_")) != -1) {
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
            case 'i':
               interactive = true;
               break;
            default:
               usage();
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
    if (argc == optind && !interactive)
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
    auto rc = XGetWindowProperty(x11, win, x11.NetFrameExtents,
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
    rc = XGetWindowProperty(x11, win, x11.NetWmDesktop, 0,
            std::numeric_limits<long>::max(), False, x11.Cardinal,
            &actualType, &actualFormat, &itemCount, &afterBytes, &prop);
    desktop = rc == 0 ? *(long *)prop : 0xffffffff;

    if (screen == -1)
        screen = x11.monitorForWindow(win);
    const Geometry *monitorGeometry;
    Geometry window;
    if (windowRelative) {
       monitorGeometry = 0;
       window = x11.getGeometry(win);

    } else {
       monitorGeometry = &x11.monitors[screen];
       window = *monitorGeometry;
       window.size.width -= frame[0] + frame[1];
       window.size.height -= frame[2] + frame[3];
       window.x += frame[0];
       window.y += frame[2];
    }

    x11.updateState(win, x11.NetWmStateShaded, X11Env::REMOVE);
    x11.updateState(win, x11.NetWmStateMaximizedHoriz, X11Env::REMOVE);
    x11.updateState(win, x11.NetWmStateFullscreen, X11Env::REMOVE);
    if (interactive) {
        auto keyWin = XCreateSimpleWindow(x11, x11.root, 0, 0, 1, 1, 0, 0, 0);
        if (keyWin == 0)
           abort();

        XMapWindow(x11, keyWin);
        XFlush(x11);
        XSelectInput(x11, keyWin, ExposureMask | KeyPressMask);

        int symsPerKey, minCodes, maxCodes;
        auto codes = XDisplayKeycodes(x11, &minCodes, &maxCodes);
        if (!codes)
           abort();
        auto keySyms = XGetKeyboardMapping(x11, minCodes, maxCodes - minCodes, &symsPerKey);

        static std::map<int, const char *> keyToOperation = {

           { XK_Up, "u" },
           { XK_Down, "d" },
           { XK_Left, "l" },
           { XK_Right, "r" },

           { XK_KP_8, "u" },
           { XK_KP_2, "d" },
           { XK_KP_4, "l" },
           { XK_KP_6, "r" },

           { XK_KP_7, "ul" },
           { XK_KP_9, "ur" },
           { XK_KP_1, "dl" },
           { XK_KP_3, "dr" },

           { XK_KP_Up, "u" },
           { XK_KP_Down, "d" },
           { XK_KP_Left, "l" },
           { XK_KP_Right, "r" },

           { XK_KP_Home, "ul" },
           { XK_KP_Page_Up, "ur" },
           { XK_KP_End, "dl" },
           { XK_KP_Page_Down, "dr" }
        };
 
        for (bool done = false; !done;) {
            XEvent event;
            XNextEvent(x11, &event);
            switch (event.type) {
               case Expose:
                  break;
               case KeyPress:
                   auto i = (event.xkey.keycode - minCodes) * symsPerKey;
                   auto todo = keyToOperation.find(keySyms[i]);
                   if (todo == keyToOperation.end())
                       exit(0);
                   resizeWindow(x11, desktop, window, win, frame, todo->second);
            }
        }
    } else {
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
        resizeWindow(x11, desktop, window, win, frame, location);
    }
}
