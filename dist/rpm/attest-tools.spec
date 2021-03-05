name:           attest-tools
Version:        0.2.92
Release:        1
Summary:        Attestation tools

Source0:        https://gitee.com/openeuler/%{name}/repository/archive/v%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
License:        GPL-2.0
Url:            https://gitee.com/openeuler/attest-tools
BuildRequires:  autoconf automake libcurl-devel libtool openssl-devel
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

%package devel
Summary: Development headers and libraries for %{name}
Requires:%{name} = %{version}-%{release}

%description devel
This package includes the headers of the libraries.

%prep
%autosetup -n %{name}-%{version} -p1

%build
autoreconf -iv
%configure
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
%make_install %{?_smp_mflags}
install -m 755 -d $RPM_BUILD_ROOT/etc/attest-tools/ek_ca_certs
install -m 755 -d $RPM_BUILD_ROOT/etc/attest-tools/privacy_ca_certs
install -m 755 -d $RPM_BUILD_ROOT/etc/sysconfig
install -m 644 etc/attest_ra_server %{buildroot}/etc/sysconfig/attest_ra_server
install -m 644 etc/attest_tls_server %{buildroot}/etc/sysconfig/attest_tls_server

%post
ldconfig

%postun
ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%dir %{_sysconfdir}/%{name}
%dir %{_sysconfdir}/%{name}/ek_ca_certs
%dir %{_sysconfdir}/%{name}/privacy_ca_certs
%{_sysconfdir}/%{name}/req_examples/*
%config(noreplace) %{_sysconfdir}/sysconfig/attest_ra_server
%config(noreplace) %{_sysconfdir}/sysconfig/attest_tls_server
%{_unitdir}/attest_ra_server.service
%{_unitdir}/attest_tls_server.service
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
%{_bindir}/get_pgp_keys.sh
%{_bindir}/generate_demoCA.sh

%files devel
%{_prefix}/include/attest-tools

%changelog
* Wed Feb 10 2021 Roberto Sassu <roberto.sassu@huawei.com> - 0.2.92-1
- Remove dependency on openssl_tpm2_engine
- Add support for PGP keys
- Move configuration files to /etc/attest-tools
- Obtain CA files from openssl configuration
- Make primary key persistent
- Add systemd units and requirements examples
- Add SKAE DATA URL extension to CSR
- Bug fixes

* Fri Nov 13 2020 Roberto Sassu <roberto.sassu@huawei.com> - 0.2.91-1
- Update algorithm for boot_aggregate calculation
- Install includes
- Bug fixes

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
