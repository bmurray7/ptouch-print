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
