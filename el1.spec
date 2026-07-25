%define abi_version 1

Name:           el1
Summary:        Essentials Library v1
Group:          System/Libraries
Distribution:   openSUSE
License:        GPLv3
URL:            https://www.brennecke-it.net

BuildRequires:  clang
BuildRequires:  lld
BuildRequires:  make
BuildRequires:  pkgconfig
BuildRequires:  valgrind-devel
BuildRequires:  pkgconfig(krb5)
BuildRequires:  pkgconfig(libpq)
BuildRequires:  pkgconfig(zlib)

%description
el1 is a C++ Essentials Library focused on IoT and high-level operations.

%prep
%setup -q -n %{name}

%build

%install
make install-runtime %{?_smp_mflags} ABI_VERSION=%{abi_version} LIB_DIR="%{buildroot}%{_libdir}"

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%license LICENSE.txt
%{_libdir}/libel1.so.%{abi_version}

%changelog
