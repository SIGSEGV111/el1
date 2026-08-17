Name:           el1-devel
Summary:        Development files for el1
Group:          Development/Libraries/C and C++
Distribution:   openSUSE
License:        GPLv3
URL:            https://www.brennecke-it.net

BuildRequires:  make
Requires:       el1 = %{version}
Requires:       krb5-devel

%description
Header files and the linker interface for developing C++ applications with el1.

%prep
%setup -q -n el1

%build

%install
make install-devel %{?_smp_mflags} PACKAGE_VERSION="%{version}" INCLUDE_DIR="%{buildroot}%{_includedir}" LIB_DIR="%{buildroot}%{_libdir}" PKG_CONFIG_DIR="%{buildroot}%{_libdir}/pkgconfig" PKG_CONFIG_LIB_DIR="%{_libdir}" PKG_CONFIG_INCLUDE_DIR="%{_includedir}"

%files
%license LICENSE.txt
%{_includedir}/el1
%{_libdir}/libel1.so
%{_libdir}/pkgconfig/el1.pc

%changelog
