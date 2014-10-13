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
        Geometry g;
        Window parent;
        WindowGeo(Window w_, Geometry g_) : w(w_), g(g_), parent(0) {}
    };

    std::vector<WindowGeo> all;

    std::transform(clients.begin(),
            clients.end(),
            std::back_insert_iterator<decltype(all)>(all),
            [&x11, &clients, &all] (const Window &w) {
                Geometry g = x11.getGeometry(w); // this is relative to its parent, probably implemented by the WM
                Window win;
                XTranslateCoordinates(x11.display, w, x11.root, g.x, g.y, &g.x, &g.y, &win);
                return WindowGeo(w, g);
    });

    std::sort(all.begin(), all.end(), [&all, x11] (const WindowGeo &lhs, const WindowGeo &rhs) {
        return std::max(lhs.g.size.height, lhs.g.size.width) - std::max(rhs.g.size.height, rhs.g.size.width);
    });

    for (const auto &item : all)
        std::cout << "{ window:" << item.w << ", geo:" << item.g << "}\n" ;
    std::cout << "monitor: " << monitor << "\n";

}
