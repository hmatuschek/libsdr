# libsdr - A simple software defined radio (SDR) library

**First of all:** I  assembled this library for my one entertainment and to learn something about software defined radio. If you are interested into a full-featured, performant SDR framework, consider using GNU radio (http://gnuradio.org). 


## Build

The only required run-time dependency of `libsdr` is `libpthread`, which is available on all Unix-like OSs like Linux and MacOS X. It is also available for windows if `mingw` is used (http://www.mingw.org) of compilation. There are also some optional dependencies, which allow for the usage of some additional features of the library. 

* `Qt5` (http://qt-project.org) - Enables the `libsdr-gui` library implementing some graphical user interface elements like a spectrum view.
* `fftw3` (http://www.fftw.org) - Also required by the GUI library and allows for FFT-convolution filters.
* `PortAudio` (http://www.portaudio.com) - Allows for sound-card input and output.
* `librtlsdr` (http://rtlsdr.org) - Allows to interface RTL2382U based USB dongles.

For the compilation of the library, `cmake` (http://www.cmake.org) is also required (as well as a compiler like gcc or clang of cause).

Compiling the library is the canonical cmake path:

```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE 
make
``` 


## License

libsdr - A simple software defined radio (SDR) library
Copyright (C) 2014 Hannes Matuschek

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
