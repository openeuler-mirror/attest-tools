# Initial setup

1) edit /etc/yum.repos.d/openEuler.repo and add:
```
[roberto]
name=roberto
baseurl=http://121.36.3.168:82/home:/roberto.sassu:/branches:/openEuler:/20.09/standard_x86_64/
enabled=1
gpgcheck=1
gpgkey=http://121.36.3.168:82/home:/roberto.sassu:/branches:/openEuler:/20.09/standard_x86_64/repodata/repomd.xml.key
```
2) hostnamectl set-hostname vm

3) edit /etc/hosts and add:
```
<VM IP>	vm
```
4) dnf install kernel openssl_tpm2_engine firewalld attest-tools digest-list-tools trousers httpd mod_ssl libcap

5) setup_grub2 measurement 4.19.140-2102.1.0.0057.oe1.x86_64

6) setup_grub2 appraisal 4.19.140-2102.1.0.0057.oe1.x86_64

7) setup_grub2 measurement+appraisal 4.19.140-2102.1.0.0057.oe1.x86_64


# Configure CA

8) generate_demoCA.sh /var/lib/demoCA --no-password

9) edit /etc/pki/tls/openssl.cnf, set dir to /var/lib/demoCA in [ CA_default ] and uncomment the line:
```
# copy_extensions = copy
```
To generate another TLS certificate for the same VM also uncomment the line:
```
# unique_subject  = no                    # Set to 'no' to allow creation of
```

# Configure RA Server

10) systemctl enable attest_ra_server

11) from the host: scp /var/lib/swtpm-localca/issuercert.pem /var/lib/swtpm-localca/swtpm-localca-rootca-cert.pem root@<VM IP>:/etc/attest-tools/ek_ca_certs

12) edit /etc/attest-tools/req_examples/req-bios-ima-sig.json and modify:
```
    "ima_sig|verify":"subject_id:b25e7f66"
```
with:
```
    "ima_sig|verify":"subject_id:b25e7f66,subject_id:0ff13748"
```
13) reboot (MEASUREMENT+APPRAISAL)


# Implicit RA

14) tail -f /var/log/attest_ra_server.log

15) attest_ra_client -s vm -a

16) attest_ra_client -p 0,1,2,3,4,5,6,7,8,9,11 -P sha1 -b -i -k -u -s vm -r /var/www/html/key_data.json -U 'http://vm/key_data.json'


# Configure Apache to use the TPM key

17) chown apache:apache /etc/attest-tools/tls_key.pem /var/www/html/key_data.json

18) gpasswd -a apache tss

19) edit /usr/lib/systemd/system/httpd.service and add:

Environment=XDG_RUNTIME_DIR=/tmp

in [Service]

20) edit /etc/httpd/conf.d/ssl.conf and set:
```
SSLCertificateFile /etc/attest-tools/tls_key_cert.pem
SSLCertificateKeyFile tpm2:/etc/attest-tools/tls_key.pem
SSLCACertificateFile /etc/attest-tools/tls_key_ca_cert.pem
```
21) systemctl enable --now httpd

22) firewall-cmd --permanent --add-service https --add-service http

23) firewall-cmd --reload


# Configure web client

24) copy /etc/attest-tools/tls_key_ca_cert.pem to the host

25) from the host: import /etc/attest-tools/tls_key_ca_cert.pem to the browser

26) from the host: access https://vm


# Implicit RA with run-time certificate verification

27) attest_tls_client -p 0,1,2,3,4,5,6,7,8,9,11 -S -s vm -r /etc/attest-tools/req_examples/req-bios-ima-sig.json -P 443 -V -D -d /etc/attest-tools/tls_key_ca_cert.pem
