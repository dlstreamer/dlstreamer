%define debug_package %{nil}
Name:           paho-mqtt-c
Version:        1.3.4
Release:        1%{?dist}
Summary:        Eclipse Paho MQTT C client library

License:        EPL-2.0 OR BSD-3-Clause
Source0:        paho.mqtt.c-%{version}.tar.gz
URL:            https://github.com/eclipse/paho.mqtt.c
Packager:       DL Streamer Team <dlstreamer@intel.com>
ExclusiveArch:  x86_64
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root

BuildRequires:  cmake gcc gcc-c++ make
BuildRequires:  openssl-devel

%description
Provides MQTT connectivity for edge AI applications.

%package devel
Summary:        Development files for %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description devel
Development headers and libraries for building applications with the Eclipse Paho MQTT C client library.

%prep
%setup -q -n paho.mqtt.c-%{version}

%build
%cmake -DPAHO_BUILD_DOCUMENTATION=FALSE -DPAHO_WITH_SSL=TRUE -DCMAKE_INSTALL_PREFIX=/usr/local
%cmake_build

%install
%cmake_install
install -D -p -m 755 %{SOURCE0} %{buildroot}%{_datadir}/%{name}/abi/paho-c.abignore
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

%files
%license LICENSE edl-v10 epl-v20
/usr/local/bin/MQTT*
/usr/local/lib/*
/usr/local/share/doc/*
/usr/share/paho-mqtt-c/abi/paho-c.abignore

%files devel
/usr/local/include/*

%changelog
* Tue Aug 25 2025 MQTT C build - 1.3.4-1
- Initial Paho MQTT C build
