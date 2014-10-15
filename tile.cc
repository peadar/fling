#include "wmhack.h"
#include <algorithm>
#include <iterator>

int
main()
{
    Display *display = XOpenDisplay(0);
    if (display == 0) {
        std::clog << "failed to open display: set DISPLAY environment variable" << std::endl;
        return 1;
    }
    X11Env x11(display);
    Window win = x11.active();
    int screen = x11.monitorForWindow(win);
    const Geometry &monitor = x11.monitors[screen];

    X11Env::WindowList clients = x11.getClientList();
    std::clog << "got " << clients.size() << " clients\n";

    struct WindowGeo {
        Window w;
        Geometry existing;
        Geometry updated;
        Window parent;
        int type;
        WindowGeo(X11Env &x11, Window w_, Geometry g_) : w(w_), existing(g_), parent(0) { }
    };

    std::vector<WindowGeo> all;
    for (auto w : clients) {
        Atom type = x11.windowType(w);
        if (type == x11.NetWmWindowTypeNormal) {
            Geometry g = x11.getGeometry(w); // this is relative to its parent, probably implemented by the WM
            Window win;
            XTranslateCoordinates(x11.display, w, x11.root, g.x, g.y, &g.x, &g.y, &win);
            all.push_back(WindowGeo(x11, w, g));
        }
    }

    std::sort(all.begin(), all.end(), [&all, x11] (const WindowGeo &lhs, const WindowGeo &rhs) {
        return std::max(lhs.existing.size.height, lhs.existing.size.width)
                        - std::max(rhs.existing.size.height, rhs.existing.size.width);
    });

    for (const auto &item : all)
        std::cout << "{ window:" << item.w << ", geo:" << item.existing << "}\n" ;
    std::cout << "monitor: " << monitor << "\n";

    Spaces s(monitor);
    for (const auto &item : all)
        x11.setGeometry(item.w, s.fit(item.existing.size));

}
