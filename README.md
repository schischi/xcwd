xcwd - X current working directory
==================================
xcwd is a simple tool which print the current working directory of the
currently focused window.

The main goal is to launch applications directly into the same directory
as the focused applications. This is especially useful if you want to open
a new terminal for debugging or compiling purpose.

Disclaimer
----------
This script **can't** retrieve the working directory of a "single instance
application" nor terminal multiplexer, e.g. :
  - tmux, screen
  - lilyterm
  - konsole
  - urxvtc with urxvtd
  - applications with tabs

The application works well with the following terminals :
  - urxvt
  - xterm
  - gnome terminal
  - terminology

How it works
------------
  - Retrieve the focused window
  - Read its attributes to get the PID. If `_NET_WM_PID` is set, xcwd just
    read the value. Otherwise it reads the `_NET_WM_CLASS` and compares it to
    the name of all the running processes
  - Search for the deepest child of the selected PID (to avoid getting the
    working directory of the terminal instead of the shell)
  - Print the current working directory

If one of those steps fail, xcwd print the content of the `HOME` variable.

Requirements
------------
  - Linux or FreeBSD
  - libX11-dev

Installation
------------
* Clone this repository or [download as ZIP](https://github.com/schischi/xcwd/archive/master.zip)
* `make`
* `make install`

Running xwcd
------------
Simply invoke the 'xcwd' command.

You probably want to use it this way:
* ``urxvt -cd "`xcwd`" ``
* ``xterm -e "cd `xcwd` && /bin/zsh"``
* ``pcmanfm "`xcwd`" ``

i3 Configuration
----------------
* bindsym $mod+Shift+Return exec ``urxvt -cd "`xcwd`" ``
* bindsym $mod+Shift+Return exec ``xterm -e "cd `xcwd` && /bin/zsh"``
* bindsym $mod+p            exec ``pcmanfm "`xcwd`"``
