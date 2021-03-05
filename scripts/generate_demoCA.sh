#! /bin/bash

# Copyright (C) 2021 Huawei Technologies Duesseldorf GmbH
#
# Author: Roberto Sassu <roberto.sassu@huawei.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, version 2 of the
# License.
#
# File: generate_demoCA.sh
#      Generate a demo CA.

if [ -z "$1" ]; then
	echo "Syntax: $0 <demoCA path> [--no-password]"
	exit 1
fi

mkdir -p $1
mkdir $1/newcerts
mkdir $1/private
echo 01 > $1/serial
touch $1/index.txt

encrypt_opt=""
if [ "$2" != "--no-password" ]; then
	encrypt_opt="-des3"
fi

echo "Generate private key..."
openssl genrsa $encrypt_opt -out $1/private/cakey.pem 4096

echo -e "\nGenerate certificate..."
openssl req -x509 -new -nodes -key $1/private/cakey.pem -sha256 -days 1024 \
            -out $1/cacert.pem

echo "Set correct dir in CA_default section of openssl.cnf \
and eventually add input_password in the same section"
