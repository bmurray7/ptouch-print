# About
ptouch is a command line tool to print labels on Brother P-Touch
printers on Linux.

There is no need to install the printer via CUPS, the printer is accessed
directly via libusb.

The tool was written for and tested with the PT-2430PC, but it should also
work with the PT-1230PC (untested so far).
Maybe others work too (please report USB VID and PID so I can include support
for further models, too).

Further info can be found at:
http://mockmoon-cybernetics.ch/computer/p-touch2430pc/

# Build
`autoreconf --install `
`make`
`sudo make install`

# U-Dev
To avoid running as root, add the line 
```
SUBSYSTEM=="usb",ATTR{idVendor}=="04f9", ATTR{idProduct}=="202d", MODE="0666"
```
to the file `/etc/udev/rules.d/99-local.rules`

# Usage
```
usage: ./ptouch-print [options] <print-command(s)>
options:
        --font <file>           use font <file> or <name>
        --writepng <file>       instead of printing, write output to png file
                                This currently works only when using
                                EXACTLY ONE --text statement
print-commands:
        --image <file>          print the given image which must be a 2 color
                                (black/white) png
        --text <text>           Print 1-4 lines of text.
                                If the text contains spaces, use quotation marks
                                around it.
        --cutmark               Print a mark where the tape should be cut
```
