/*
 * Copyright (C) 2018-2019 Huawei Technologies Duesseldorf GmbH
 *
 * Author: Roberto Sassu <roberto.sassu@huawei.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * File: ima_cp.c
 *      Add measured files to data context
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

#include "ctx.h"
#include "util.h"
#include "event_log/ima.h"

#include <sys/mman.h>
#include <sys/wait.h>

#define IMA_CP_ID "ima_cp|verify"
#define PGP_SCRIPT "/usr/bin/get_pgp_keys.sh"

int verify(attest_ctx_data *d_ctx, attest_ctx_verifier *v_ctx)
{
	struct event_log *bios_log, *ima_log;
	struct event_log_entry *cur_log_entry;
	struct ima_log_entry *ima_log_entry;
	const char *data_ptr;
	uint32_t data_len;
	unsigned char *file_content;
	size_t file_content_len;
	DIR *dir;
	struct dirent *d_entry;
	char path[PATH_MAX];
	int rc = 0;

	bios_log = attest_event_log_get(v_ctx, "bios");
	if (bios_log)
		list_for_each_entry(cur_log_entry, &bios_log->logs, list)
			cur_log_entry->flags |= LOG_ENTRY_PROCESSED;

	ima_log = attest_event_log_get(v_ctx, "ima");
	if (!ima_log)
		return -ENOENT;

	if (fork() == 0)
		return execlp(PGP_SCRIPT, PGP_SCRIPT, NULL);

	wait(NULL);

	dir = opendir("/etc/keys");
	if (!dir)
		return -EACCES;

	while ((d_entry = readdir(dir))) {
		if (!strcmp(d_entry->d_name, ".") ||
		    !strcmp(d_entry->d_name, ".."))
			continue;

		snprintf(path, sizeof(path), "/etc/keys/%s", d_entry->d_name);

		rc = attest_util_read_file(path, &file_content_len,
					   &file_content);
		if (!rc) {
			rc = attest_ctx_data_add_copy(d_ctx, CTX_AUX_DATA,
					file_content_len, file_content,
					d_entry->d_name);
			munmap(file_content, file_content_len);
		}
	}

	closedir(dir);

	list_for_each_entry(cur_log_entry, &ima_log->logs, list) {
		ima_log_entry = (struct ima_log_entry *)cur_log_entry->log;
		cur_log_entry->flags |= LOG_ENTRY_PROCESSED;

		rc = ima_template_get_field(ima_log_entry, FIELD_SIG, &data_len,
					    (const unsigned char **)&data_ptr);
		if (!rc && data_len)
			continue;

		rc = ima_template_get_eventname(ima_log_entry, &data_len,
						&data_ptr);
		if (rc)
			break;

		if (!strncmp(data_ptr, "boot_aggregate", data_len))
			continue;

		rc = attest_util_read_file(data_ptr, &file_content_len,
					   &file_content);
		if (!rc) {
			rc = attest_ctx_data_add_copy(d_ctx, CTX_AUX_DATA,
					file_content_len, file_content,
					basename(data_ptr));
			munmap(file_content, file_content_len);
		} else if (rc == -ENOENT) {
			rc = 0;
		}
	}

	return rc;
}

int num_func = 1;

struct verifier_struct func_array[1] = {{.id = IMA_CP_ID, .func = verify}};
