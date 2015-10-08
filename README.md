# Fling an X11 window around

## Usage

## Move a window on the screen:

- fling *\[-x\]* *\[ window selection\]* *\[ -s <screen> ]\* *\[-b <border>\]*
  *\[window-motion\]*
   - *-b \<num\>* : specify border width (pixels)
   - *-s* : specify the xinerama monitor to move the window to.
   - *-x* : for window motion commands, start with the window's current geometry, rather than the full monitor
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
   - window motion: control string: *\<\[percent\]u|d|l|r|v|h\>+*
       - The window is initially sized to the entire monitor, *percent*
         defaults to 50. each character reduces it in some way patterns can
         repeat, so, for example, *fling 30dl* will place the window in the
         left half of the lower 30% of the monitor
         - l: retain only the left *percent* of the space currently occupied
         - r: retain only the right *percent* of the space currently occupied
         - u: retain only the top *percent* space currently occupied
         - d: retain only the bottom *percent* space currently occupied
         - h: retain *percent* of the existing width, and centre in the
           existing horizontal space
         - v: retain *percent* of the existing height, and centre in the
           existing vertical space

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
