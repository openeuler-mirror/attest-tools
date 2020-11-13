name:           attest-tools
Version:        0.2.90
Release:        2
Summary:        Attestation tools

Source0:        https://gitee.com/openeuler/%{name}/repository/archive/v%{version}.tar.gz
Source1:	openssl_tpm2_engine-2.4.2.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
License:        GPL-2.0
Url:            https://gitee.com/openeuler/attest-tools
BuildRequires:  autoconf automake libcurl-devel libtool
BuildRequires:  digest-list-tools json-c-devel libcurl-devel tss2-devel
Requires:       json-c curl tss2

%if 0%{?suse_version}
BuildRequires:  libopenssl-devel
%else
BuildRequires:  openssl-devel
%endif

%description
This package includes the tools to perform remote attestation with a quote
or a TPM key.

%prep
%autosetup -n %{name}-%{version} -p1
%setup -a 1 -n %{name}-%{version}

%build
autoreconf -iv
%configure
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
%make_install %{?_smp_mflags}

%post
ldconfig

%postun
ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_libdir}/libenroll_client.so
%{_libdir}/libverifier_ima_policy.so
%{_libdir}/libskae.so
%{_libdir}/libverifier_bios.so
%{_libdir}/libattest.so
%{_libdir}/libverifier_dummy.so
%{_libdir}/libenroll_server.so
%{_libdir}/libverifier_ima_cp.so
%{_libdir}/libverifier_ima_sig.so
%{_libdir}/libverifier_evm_key.so
%{_libdir}/libeventlog_ima.so
%{_libdir}/libverifier_ima_boot_aggregate.la
%{_libdir}/libverifier_ima_boot_aggregate.so
%{_libdir}/libeventlog_bios.so
%exclude %{_libdir}/*.la
%{_bindir}/attest_tls_client
%{_bindir}/attest_build_json
%{_bindir}/attest_tls_server
%{_bindir}/attest_ra_server
%{_bindir}/attest_ra_client
%{_bindir}/attest_create_skae
%{_bindir}/attest_certify.sh
%{_bindir}/ekcert_read.sh
%{_bindir}/attest_parse_json


%changelog
* Mon Sep 14 2020 Roberto Sassu <roberto.sassu@huawei.com> - 0.2.90-2
- Change Source0 in spec file

* Wed Jul 08 2020 Roberto Sassu <roberto.sassu@huawei.com> - 0.2.90
- Bug fixes

* Thu Dec 12 2019 Roberto Sassu <roberto.sassu@huawei.com> - 0.2.0
- Add quote protocol
- Add parser for TPM 2.0 event log
- Add evm_key and dummy verifiers
- Add creation of certificate for TPM key
- Add creation of symmetric key to be used with EVM
- Add ima-sig and ima-cp verifiers

* Fri Aug 16 2019 Roberto Sassu <roberto.sassu@huawei.com> - 0.1.0
- First public release
