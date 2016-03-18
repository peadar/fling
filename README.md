# Fling an X11 window around

## Usage

## Move a window on the screen:

- fling *\[-x\]* *\[ window selection\]* *\[ -s <screen> \]* *\[-b <border>\]*
  ( *\[window-motion\]* | *-i* )
   - *-b \<num\>* : specify border width (pixels)
   - *-s* : specify the xinerama monitor to move the window to.
   - *-x* : for window motion commands, start with the window's current geometry, rather than the full monitor
   - *-g* : disable "glide" window/smooth motion
   - window selection:
      - *-w \<window-id\>* : specify explicit integer window id.
      - *-p* : select with mouse pointer
   - window motion:  move to specified area of screen. One of:
     - *left*
     - *right*
     - *top*
     - *bottom*
     - *topleft*
     - *bottomleft*
     - *topright*
     - *bottomright*
   - window motion: control string: *\<\[\[numerator'/'\]denominator\]u|d|l|r|v|h\>+*
       - The window is initially sized to the entire monitor. numerator defaults to 1,
         and denominator to 2, and forms *fraction*
         defaults to 1/2. each character reduces it in some way patterns can
         repeat, so, for example, *fling 2/3dl* will place the window in the
         left half of the lower two thirds of the monitor
         - l: retain only the left *fraction* of the space currently occupied
         - r: retain only the right *fraction* of the space currently occupied
         - u: retain only the top *fraction* space currently occupied
         - d: retain only the bottom *fraction* space currently occupied
         - h: retain *fraction* of the existing width, and centre in the
           existing horizontal space
         - v: retain *fraction* of the existing height, and centre in the
           existing vertical space
   - window motion: -i
      - An X11 window is opened, and keyboard input solicited. Cursor keys
        fling the window up, down, left right. Numeric keypad does same,
        with home, pageup, end, and pagedn flinging to corners. Any
        other key exits fling.

## Window manager interactions: 
  *   *-p*        : use the mouse to pick the window to fling once invoked.
  *   *-f*        : toggle "fullscreen"
  *   *-m*        : toggle "maximised"
  *   *-u*        : toggle "below other windows"
  *   *-a*        : toggle "above other windows"
  *   *-h*        : toggle "sHaded"
  *   *-x*        : use the window's existing dimensions as the starting
      geometry
  *   *-o \<num\>*: set window opacity
