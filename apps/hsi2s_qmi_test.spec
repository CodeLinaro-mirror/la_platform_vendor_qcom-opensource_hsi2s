Name: hsi2s-qmi-test
Version: 1.0
Release: r0
Summary: HSI2S application
License: BSD-3-Clause-Clear
Source0: %{name}-%{version}.tar.gz

%define DEPENDS hsi2s-qmi
BuildRequires: autoconf automake libtool gcc-g++ hsi2s-qmi
Requires: %{DEPENDS}


%description
HSI2S application

%prep
%autosetup -n %{name}-%{version}

%build
%set_build_flags
make


%install
mkdir -p %{buildroot}/%{_bindir}
install -m 0755 hsi2s_qmi_test %{buildroot}/%{_bindir}

%files
%{_bindir}/hsi2s_qmi_test
