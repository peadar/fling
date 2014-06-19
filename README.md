# Fling an X11 window around

Use

    $ fling topleft

and

    $ fling bottom

and other obvious combinations of top bottom, left, and right to get
the active window shifted to one of 8 segments of the screen

If the argument does not match the above, then it is considered a control string. In this mode, the window is initially sized to the entire monitor, and each character reduces it in some way:

l: retain only the left 50% of the space currently occupied
r: retain only the right 50% of the space currently occupied
u: retain only the top 50% space currently occupied
d: retain only the bottom 50% space currently occupied
h: retain 50% of the existing width, and centre in the existing horizontal space
v: retain 50% of the existing height, and centre in the existing vertical space

More control is available using fraction-like notation:

    $ fling 1/3 2/2

makes the window occupy the first third of the X axis, and the 2nd half
of the Y axis (ie, bottom left, but only 1/3rd the width of the screen)

You can add a ":" after the fractional bit to indicate a span, eg:

    $ fling 1/3:2 2/2

Makes the window shift to the bottom left, occupying the bottom half,
and leftmost two-thirds of the screen.

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
