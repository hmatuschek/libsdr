Summary: A C++ library for software defined radios

%define version 0.1.0

License: GPL-2.0+
Name: libsdr 
Group: Development/Libraries/C and C++
Prefix: /usr
Release: 1
Source: libsdr-%{version}.tar.gz
URL: https://github.com/hmatuschek/libsdr
Version: %{version}
Buildroot: /tmp/libsdrrpm
BuildRequires: gcc-c++, cmake, portaudio-devel, fftw3-devel, librtlsdr-devel
Requires: portaudio, fftw3, librtlsdr-devel
%if 0%{?suse_version}
BuildRequires: libqt5-qtbase-devel
Requires: libqt5-qtbase 
%endif
%if 0%{?fedora}
BuildRequires: qt5-qtbase-devel
Requires: qt5-qtbase 
%endif

%description
A simple C++ library for software defined radios.


%package -n libsdr-devel
Summary: A C++ library for software defined radios
Group: Development/Libraries/C and C++
BuildRequires: gcc-c++, cmake, portaudio-devel, fftw3-devel, librtlsdr-devel
Requires: portaudio, fftw3, librtlsdr
%if 0%{?suse_version}
BuildRequires: libqt5-qtbase-devel
Requires: libqt5-qtbase 
%endif
%if 0%{?fedora}
BuildRequires: qt5-qtbase-devel
Requires: qt5-qtbase 
%endif
 
%description -n libsdr-devel
Provides the runtime library header files for libsdr.

%prep
%setup -q


%build
cmake -DCMAKE_BUILD_TYPE=RELEASE \
      -DCMAKE_INSTALL_PREFIX=$RPM_BUILD_ROOT/usr
make


%install
make install

%clean
rm -rf $RPM_BUILD_ROOT

%post
/sbin/ldconfig
 
%postun
/sbin/ldconfig


%files
%{_libdir}/libsdr.so.*
%{_libdir}/libsdr-gui.so.*

%files -n libsdr-devel
%defattr(-, root, root, -)
%{_libdir}/libsdr.so
%{_libdir}/libsdr-gui.so
%{_includedir}/libsdr/
