Name:           el1
Summary:        Essentials Library v1
Group:          System/Libraries
Distribution:   openSUSE
License:        GPLv3
URL:            https://www.brennecke-it.net

BuildRequires:  bluez-devel
BuildRequires:  clang
BuildRequires:  lld
BuildRequires:  make
BuildRequires:  pkgconfig
BuildRequires:  valgrind-devel
BuildRequires:  libstdc++-devel
BuildRequires:  pkgconfig(krb5)
BuildRequires:  pkgconfig(libpq)
BuildRequires:  pkgconfig(openssl) >= 3.5
BuildRequires:  pkgconfig(libnghttp2)
Requires:       libopenssl3 >= 3.5
Requires:       libnghttp2-14
BuildRequires:  pkgconfig(zlib)

%description
el1 is a C++ Essentials Library focused on IoT and high-level operations.

%prep
%setup -q -n %{name}

%build

%install
make install-runtime %{?_smp_mflags} PACKAGE_VERSION="%{version}" LIB_DIR="%{buildroot}%{_libdir}"

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%license LICENSE.txt
%{_libdir}/libel1.so.%{version}

%changelog
