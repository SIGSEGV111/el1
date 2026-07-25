%define abi_version 1

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
make install-devel %{?_smp_mflags} ABI_VERSION=%{abi_version} INCLUDE_DIR="%{buildroot}%{_includedir}" LIB_DIR="%{buildroot}%{_libdir}"

%files
%license LICENSE.txt
%{_includedir}/el1
%{_libdir}/libel1.so

%changelog
