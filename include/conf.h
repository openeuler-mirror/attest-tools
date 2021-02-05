/*
 * Copyright (C) 2021 Huawei Technologies Duesseldorf GmbH
 *
 * Author: Roberto Sassu <roberto.sassu@huawei.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * File: conf.h
 *      General configuration.
 */

#ifndef _CONF_H
#define _CONF_H

#define ATTEST_TOOLS_CONF_DIR "/etc/attest-tools/"
#define EK_CA_DIR ATTEST_TOOLS_CONF_DIR "ek_ca_certs/"
#define PRIVACY_CA_DIR ATTEST_TOOLS_CONF_DIR "privacy_ca_certs/"
#define PRIVACY_CA_CERT_PATH PRIVACY_CA_DIR "privacy_ca_cert.pem"
#define AK_PRIV_PATH ATTEST_TOOLS_CONF_DIR "ak_priv.bin"
#define AK_PUB_PATH ATTEST_TOOLS_CONF_DIR "ak_pub.bin"
#define AK_CERT_PATH ATTEST_TOOLS_CONF_DIR "ak_cert.pem"
#define TLS_KEY_PRIV_PATH ATTEST_TOOLS_CONF_DIR "tls_key_priv.bin"
#define TLS_KEY_PUB_PATH ATTEST_TOOLS_CONF_DIR "tls_key_pub.bin"
#define TLS_KEY_CERT_PATH ATTEST_TOOLS_CONF_DIR "tls_key_cert.pem"
#define TLS_KEY_CA_CERT_PATH ATTEST_TOOLS_CONF_DIR "tls_key_ca_cert.pem"
#define OPENSSL_TPM2_KEY_PATH ATTEST_TOOLS_CONF_DIR "tls_key.pem"
#define SYM_KEY_PRIV_PATH ATTEST_TOOLS_CONF_DIR "sym_key_priv.bin"
#define SYM_KEY_PUB_PATH ATTEST_TOOLS_CONF_DIR "sym_key_pub.bin"
#define SYM_KEY_POLICY_PATH ATTEST_TOOLS_CONF_DIR "sym_key_policy.txt"
#define SYM_KEY_BLOB ATTEST_TOOLS_CONF_DIR "trusted_key.blob"

#endif /*_CONF_H*/
