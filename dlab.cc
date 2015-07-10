#include "wmhack.h"
#include <err.h>

static int intarg() { return atoi(optarg); } // XXX: use strtol and invoke usage()

static void
usage()
{
    std::clog
<< "usage:" << std::endl
<< "dlab [ -d N ] label" << std::endl;
    exit(1);
}

int
main(int argc, char *argv[])
{
    int desktop = -1;
    int c;

    std::list<Atom> toggles;

    Display *display = XOpenDisplay(0);
    if (display == 0) {
        std::clog << "failed to open display: set DISPLAY environment variable"
                  << std::endl;
        return 1;
    }
    X11Env x11(display);

    while ((c = getopt(argc, argv, "d:")) != -1) {
        switch (c) {
            case 'd':
                desktop = intarg();
                break;
            default:
                usage();
                break;
        }
    }

    unsigned long itemCount, afterBytes;
    unsigned char *desktopProperty = 0;
    int actualFormat;
    Atom actualType;
    int rc;

    if (desktop == -1) {
        rc = XGetWindowProperty(x11.display, x11.root, x11.NetCurrentDesktop,
                           0, 1, False,
                           x11.Cardinal, &actualType, &actualFormat,
                           &itemCount, &afterBytes, &desktopProperty);
        if (rc != Success || itemCount < 1)
            err(1, "no current desktop");
        desktop = *(long *)desktopProperty;
        XFree(desktopProperty);
    }

    unsigned char *desktopNames;
    rc = XGetWindowProperty(x11.display, x11.root, x11.NetDesktopNames, 0,
                            std::numeric_limits<long>::max(), False,
                            AnyPropertyType, &actualType, &actualFormat,
                            &itemCount, &afterBytes, &desktopNames);
    if (rc != Success) {
        err(1, "can't get desktop names");
    }

    bool show = optind == argc;
    size_t newTitleLen = 0;
    for (int i = optind; i < argc; ++i)
        newTitleLen += strlen(argv[i]) + 1;

    size_t newBufLen = 0;
    int i;
    const char *from = (const char *)desktopNames;
    const char *fromp;
    for (i = 0, fromp = from; *fromp; fromp += strlen(fromp) + 1, ++i) {
        if (show)
            std::cout << (i == desktop ? "* " : "  ") << fromp << "\n";
        else
            newBufLen += i == desktop ? newTitleLen : strlen(fromp) + 1;
    }
    if (show)
        exit(0);
    newBufLen++; // for terminating 0.

    char *to = new char[newBufLen];

    char *top;
    for (i = 0, top = to, fromp = from; *fromp; fromp += strlen(fromp) + 1, ++i) {
        if (i == desktop) {
            for (int j = optind; j < argc; ++j) {
                if (j != optind)
                    *top++ = ' ';
                top += sprintf(top, "%s", argv[j]);
            }
        } else {
            top += sprintf(top, "%s", fromp);
        }
        *top++ = 0;
    }
    *top++ = 0;
    assert(size_t(top - to) == newBufLen);
    XChangeProperty(x11.display, x11.root, x11.NetDesktopNames, actualType,
                    actualFormat, PropModeReplace, (unsigned char *)to,
                    newBufLen);
    XFlush(x11.display);
    return 0;
}
