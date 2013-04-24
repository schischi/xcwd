xcwd - X current working directory
==================================
xcwd is a simple tool which print the current working directory of the
currently focused window.
The main goal is to launch applications directly into the same directory
as the focused applications. This is especially useful if you want to open
a new terminal for debugging or compiling purpose.

Requirements
------------
- libX11-dev

Running xwcd
------------
Simply invoke the 'xcwd' command.
You probably want to use it this way:
    ``urxvt -cd `xcwd` ``
