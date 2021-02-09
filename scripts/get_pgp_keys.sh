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
# File: get_pgp_keys.sh
#      Get PGP keys from RPM database.

if [ -z $(which rpm) ] || [ -z $(which gpg) ]; then
	exit 0
fi

mkdir -p /etc/keys

for rpm in $(rpm -qa |grep gpg-pubkey); do
	if [ -f /etc/keys/pgp-key-${rpm#gpg-pubkey-} ]; then
		continue
	fi

	rpm -qi $rpm | gpg --dearmor > /etc/keys/pgp-key-${rpm#gpg-pubkey-}
done
