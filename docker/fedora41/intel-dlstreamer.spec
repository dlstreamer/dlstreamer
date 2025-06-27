Name:           intel-dlstreamer
Version:        DLSTREAMER_VERSION
Release:        1%{?dist}
Summary:        Deep Learning Streamer

License:        Proprietary
Source0:        %{name}-%{version}.tar.gz
URL:            https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer
Packager:       DL Streamer Team <dlstreamer@intel.com>
ExclusiveArch: x86_64
AutoReqProv:    no
%define debug_package %{nil}
%define __os_install_post %{nil}

Requires: glib2-devel
Requires: libjpeg-turbo-devel
Requires: libdrm
Requires: wayland-devel
Requires: libX11-devel
Requires: libpng-devel
Requires: libva
Requires: libcurl-devel
Requires: libvorbis-devel
Requires: libde265-devel
Requires: libXext-devel
Requires: mesa-libGL
Requires: mesa-libGLU
Requires: libgudev1-devel
Requires: wget
Requires: bzip2
Requires: ffmpeg
Requires: paho-c-devel
Requires: python3
Requires: python3-devel
Requires: python3-pip
Requires: python3-gobject
Requires: python3-gobject-devel
Requires: cairo-devel
Requires: gcc
Requires: gobject-introspection-devel
Requires: libvpx-devel
Requires: opus-devel
Requires: libsrtp-devel
Requires: libXv-devel
Requires: libva-utils
Requires: openexr
Requires: tbb
Requires: intel-media-driver
Requires: openvino-2025.1.0

%description
This package contains Intel DL Streamer.

%prep
%setup -q

%build
# No build steps needed

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/opt
mkdir -p %{buildroot}/usr

cp -a opt/* %{buildroot}/opt/

find %{buildroot} -type f \( -name "*.so*" -o -perm -111 \) | while read -r file; do
    if patchelf --print-rpath "$file" &>/dev/null; then
        rpath=$(patchelf --print-rpath "$file")
        if [ -n "$rpath" ]; then
            echo "Removing RPATH from $file"
            patchelf --remove-rpath "$file"
        fi
    fi
done

%check
# No test suite

%files
%license LICENSE
/opt/intel/dlstreamer/
/opt/opencv/
/opt/openh264/
/opt/rdkafka/
/opt/ffmpeg/

%changelog
* CURRENT_DATE_TIME DL Streamer Team <dlstreamer@intel.com> - DLSTREAMER_VERSION
- Initial release.
