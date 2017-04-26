# win-shaper
Windows traffic-shaping packet filter

This tool allows you to emulate various network conditions. For more information, see [the blog post](http://calendar.perfplanet.com/2016/testing-with-realistic-networking-conditions/).

IMPORTANT: The released drivers are code signed but not with an EV certificate so they will not work in WIndows 10 with secureboot enabled.  You must disable secureboot to use the released binaries from here directly (if you build it yourself with EV signing and go through the process the restrictions will not apply).
