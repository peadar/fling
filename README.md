# Fling an X11 window around

Use

    $ fling topleft

and

    $ fling bottom

and other obvious combinations of top bottom, left, and right to get
the active window shifted to one of 8 segments of the screen

If the argument does not match the above, then it is considered a control string. In this mode, the window is initially sized to the entire monitor, and each character reduces it in some way:

* l: retain only the left 50% of the space currently occupied
* r: retain only the right 50% of the space currently occupied
* u: retain only the top 50% space currently occupied
* d: retain only the bottom 50% space currently occupied
* h: retain 50% of the existing width, and centre in the existing horizontal space
* v: retain 50% of the existing height, and centre in the existing vertical space

You can precede each of these characters with a percentage of the window to leave, eg: 

    $ fling 30d

makes the window occupy the bottom 30% of the monitor.

xinerama is supported: use "-s \<num\>" to shift from the current xinerama
monitor.

Any \_NET\_WM\_STRUT\_PARTIAL hints on windows will be obeyed as much as
possible.

You can use the following flags:

*   "-p"        : use the mouse to pick the window to fling once invoked.
*   "-f"        : toggle "fullscreen"
*   "-m"        : toggle "maximised"
*   "-u"        : toggle "below other windows"
*   "-a"        : toggle "above other windows"
*   "-h"        : toggle "sHaded"
*   "-b \<num\>"  : specify border width (pixels)
