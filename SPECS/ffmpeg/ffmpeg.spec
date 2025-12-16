%define debug_package %{nil}
Name:           ffmpeg
Version:        6.1.1
Release:        1%{?dist}
Summary:        Intel optimized FFmpeg build with VAAPI support

License:        LGPL-2.1+
Source0:        ffmpeg-%{version}.tar.gz
URL:            https://ffmpeg.org/
Packager:       DL Streamer Team <dlstreamer@intel.com>
ExclusiveArch:  x86_64
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root

BuildRequires:  gcc gcc-c++ make yasm nasm
BuildRequires:  libva-devel libva-intel-media-driver
BuildRequires:  libX11-devel libXext-devel
#BuildRequires:  x264-devel x265-devel
#BuildRequires:  openssl-devel

Requires:       libva2 libva-intel-media-driver
#Requires:       libX11 libXext libXv
#Requires:       libvpx opus
#Requires:       x264-libs x265-libs

%description
Intel optimized FFmpeg build with VAAPI hardware acceleration support.
This version is specifically configured for use with Intel DL Streamer.

%package devel
Summary:        Development files for %{name}
Requires:       %{name} = %{version}-%{release}

%description devel
Development files and headers for Intel FFmpeg.

%prep
%setup -q -n ffmpeg-%{version}

%build
./configure --enable-pic \
            --enable-shared \
            --enable-static \
            --enable-avfilter \
            --enable-vaapi \
            --extra-cflags="-I/include" \
            --extra-ldflags="-L/lib" \
            --extra-libs=-lpthread \
            --extra-libs=-lm \
            --bindir="/bin"

make -j "$(nproc)"

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

# Remove RPATH for all binaries/libs
find %{buildroot} -type f \( -name "*.so*" -o -perm -111 \) | while read -r file; do
    if patchelf --print-rpath "$file" &>/dev/null; then
        rpath=$(patchelf --print-rpath "$file")
        if [ -n "$rpath" ]; then
            echo "Removing RPATH from $file"
            patchelf --remove-rpath "$file"
        fi
    fi
done

%clean
rm -rf %{buildroot}

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%doc README.md LICENSE.md
%license COPYING.LGPLv2.1
/bin/*
/usr/local/lib/*
/usr/local/share/*

%files devel
%defattr(-,root,root,-)
/usr/local/include/*

%changelog
* Thu Aug 25 2025 ffmpeg build - 6.1.1-1
- Initial ffmpeg build
