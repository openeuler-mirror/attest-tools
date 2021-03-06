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
 * File: ctx.c
 *      Context functions.
 */

/**
 * @defgroup developer-api Developer API
 * @brief
 * Functions for developers to extend the functionality provided by the library.
 */

/**
 * @defgroup app-api Application API
 * @brief
 * Functions for applications using the library
 */

/**
 * @defgroup context-api Context API
 * @ingroup developer-api
 * @brief
 * Functions to store data used by other APIs and to store the status of a
 * verification.
 */

/**
 * @addtogroup context-api
 *  @{
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>

#include <sys/mman.h>

#include "ctx.h"
#include "util.h"

#define TEMP_DIR_TEMPLATE "/tmp/attest-temp-dir-XXXXXX"
#define TEMP_FILE_TEMPLATE "attest-temp-file-XXXXXX"

#define MAX_DIGEST_SIZE 128
#define MAX_LOG_LENGTH 1024

attest_ctx_data global_ctx_data = {0};
attest_ctx_verifier global_ctx_verifier = {0};

struct verification_log unknown_log = {{&unknown_log.list, &unknown_log.list},
					"unknown log", "fail",
					"unknown_reason"};

static const char *ctx_fields_str[CTX__LAST] = {
	[CTX_PRIVACY_CA_CERT] = "privacy_ca_cert",
	[CTX_AK_CERT] = "ak_cert",
	[CTX_TPM_AK_KEY] = "tpm_ak",
	[CTX_TPM_KEY_TEMPLATE] = "tpm_key",
	[CTX_TPM_KEY_POLICY] = "policy",
	[CTX_SYM_KEY_POLICY] = "sym_policy",
	[CTX_EVENT_LOG] = "event_log",
	[CTX_AUX_DATA] = "aux_data",
	[CTX_EK_CERT] = "ek_cert",
	[CTX_EK_CA_CERT] = "ek_ca_cert",
	[CTX_CRED] = "cred",
	[CTX_CRED_HMAC] = "cred_hmac",
	[CTX_CREDBLOB] = "credblob",
	[CTX_SECRET] = "secret",
	[CTX_CSR] = "csr",
	[CTX_KEY_CERT] = "key_cert",
	[CTX_CA_CERT] = "ca_cert",
	[CTX_HOSTNAME] = "hostname",
	[CTX_TPM_SYM_KEY] = "tpm_sym_key",
	[CTX_NONCE] = "nonce",
	[CTX_NONCE_HMAC] = "nonce_hmac",
	[CTX_TPMS_ATTEST] = "tpms_attest",
	[CTX_TPMS_ATTEST_SIG] = "tpms_attest_sig",
};

static const char *data_formats_str[DATA_FMT__LAST] = {
	[DATA_FMT_BASE64] = "base64",
	[DATA_FMT_URI] = "uri",
};

/**
 * @name Data Context API
 *  @{
 */

/**
 * Get field name
 * @param[in] field	field identifier
 *
 * @returns field name on success, NULL if not found
 */
const char *attest_ctx_data_get_field(enum ctx_fields field)
{
	return ctx_fields_str[field];
}

/**
 * Get data format name
 * @param[in] fmt	data format identifier
 *
 * @returns data format on success, NULL if not found
 */
const char *attest_ctx_data_get_format(enum data_formats fmt)
{
	return data_formats_str[fmt];
}

static int attest_ctx_data_lookup_common(const char *data, int data_len,
					 int last, const char *string_array[])
{
	int i;

	for (i = 0; i < last; i++) {
		if ((data_len && !strncmp(data, string_array[i], data_len)) ||
		    (!data_len && !strcmp(data, string_array[i])))
			return i;
	}

	return last;
}

/**
 * Get field identifier
 * @param[in] field	field name
 *
 * @returns field identifier on success, CTX__LAST if not found
 */
enum ctx_fields attest_ctx_data_lookup_field(const char *field)
{
	return attest_ctx_data_lookup_common(field, 0, CTX__LAST,
					     ctx_fields_str);
}

/**
 * Get data format identifier
 * @param[in] fmt	data format name
 * @param[in] fmt_len	data format name length
 *
 * @returns data format identifier on success, DATA_FMT__LAST if not found
 */
enum data_formats attest_ctx_data_lookup_format(const char *fmt, int fmt_len)
{
	return attest_ctx_data_lookup_common(fmt, fmt_len, DATA_FMT__LAST,
					     data_formats_str);
}

static int attest_ctx_data_add_common(attest_ctx_data *ctx,
				      enum ctx_fields field, char *path,
				      size_t len, unsigned char *data,
				      const char *label)
{
	struct data_item *new_item = NULL;
	char path_dest[MAX_PATH_LENGTH], *path_ptr = path;
	char *filename;
	int rc = -EINVAL;

	if (!ctx)
		return -EINVAL;

	if (path) {
		filename = strrchr(path, '/');
		if (filename)
			filename++;
		else
			filename = path;

		if (strncmp(path, ctx->data_dir, strlen(ctx->data_dir))) {
			snprintf(path_dest, sizeof(path_dest), "%s/%s",
				 ctx->data_dir, filename);
			rc = attest_util_copy_file(path, path_dest);
			if (rc)
				goto out;

			path_ptr = path_dest;
		}

		rc = attest_util_read_file(path_ptr, &len, &data);
		if (rc)
			goto out;

		path_ptr = strdup(path_ptr);
		if (!path_ptr) {
			rc = -ENOMEM;
			goto out;
		}
	}

	if (!data)
		goto out;

	new_item = calloc(1, sizeof(*new_item));
	if (!new_item) {
		rc = -ENOMEM;
		goto out;
	}

	new_item->data = data;
	new_item->len = len;
	new_item->mapped_file = path_ptr;

	if (label) {
		new_item->label = strdup(label);
		if (!new_item->label) {
			rc = -ENOMEM;
			goto out;
		}
	}

	list_add_tail(&new_item->list, &ctx->ctx_data[field]);
	rc = 0;
out:
	if (rc) {
		if (path)
			munmap(data, len);

		if (new_item) {
			free(new_item->label);
			free(new_item);
		}
	}

	return rc;
}

/**
 * Add binary data to data context
 * @param[in] ctx	data context
 * @param[in] field	field identifier
 * @param[in] len	binary data length
 * @param[in] data	binary data
 * @param[in] label	data label
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_data_add(attest_ctx_data *ctx, enum ctx_fields field,
			size_t len, unsigned char *data, const char *label)
{
	return attest_ctx_data_add_common(ctx, field, NULL, len, data, label);
}

/**
 * Copy and add binary data to data context
 * @param[in] ctx	data context
 * @param[in] field	field identifier
 * @param[in] len	binary data length
 * @param[in] data	binary data
 * @param[in] label	data label
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_data_add_copy(attest_ctx_data *ctx, enum ctx_fields field,
			     size_t len, unsigned char *data,
			     const char *label)
{
	unsigned char *copy;

	copy = malloc(len);
	if (!copy)
		return -ENOMEM;

	memcpy(copy, data, len);

	return attest_ctx_data_add_common(ctx, field, NULL, len, copy, label);
}

/**
 * Add file to data context
 * @param[in] ctx	data context
 * @param[in] field	field identifier
 * @param[in] path	file path
 * @param[in] label	data label
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_data_add_file(attest_ctx_data *ctx, enum ctx_fields field,
			     char *path, const char *label)
{
	return attest_ctx_data_add_common(ctx, field, path, 0, NULL, label);
}

/**
 * Add directory to data context
 * @param[in] ctx	data context
 * @param[in] field	field identifier
 * @param[in] dir_path	directory path
 * @param[in] label	data label
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_data_add_dir(attest_ctx_data *ctx, enum ctx_fields field,
			    char *dir_path, const char *label)
{
	char file_path[PATH_MAX];
	struct dirent *d_entry;
	DIR *dir;
	int rc;

	dir = opendir(dir_path);
	if (!dir)
		return -EACCES;

	while ((d_entry = readdir(dir))) {
		if (!strcmp(d_entry->d_name, ".") ||
		    !strcmp(d_entry->d_name, ".."))
                        continue;

		snprintf(file_path, sizeof(file_path), "%s/%s", dir_path,
			 d_entry->d_name);

		rc = attest_ctx_data_add_common(ctx, field, file_path, 0, NULL,
						label);
		if (rc)
			break;
	}

	closedir(dir);
	return 0;
}

/**
 * Add string \<fmt\>:\<data\> to data context
 * @param[in] ctx	data context
 * @param[in] field	field identifier
 * @param[in] string	data string
 * @param[in] label	data label
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_data_add_string(attest_ctx_data *ctx, enum ctx_fields field,
			       const char *string, const char *label)
{
	char data_path_template[MAX_PATH_LENGTH], *data_sep;
	unsigned char *output;
	enum data_formats fmt;
	size_t output_len;
	int rc = 0, fd;

	if (!ctx)
		return -EINVAL;

	data_sep = strchr(string, ':');
	if (!data_sep)
		return -EINVAL;

	fmt = attest_ctx_data_lookup_format(string, data_sep - string);
	if (fmt == DATA_FMT__LAST)
		return -EINVAL;

	snprintf(data_path_template, sizeof(data_path_template), "%s/%s",
		 ctx->data_dir, (label && field == CTX_AUX_DATA) ?
		 label : TEMP_FILE_TEMPLATE);

	if (label && field == CTX_AUX_DATA)
		fd = open(data_path_template, O_WRONLY | O_CREAT, 0600);
	else
		fd = mkstemp(data_path_template);

	if (fd < 0)
		return -EACCES;

	switch (fmt) {
	case DATA_FMT_BASE64:
		rc = attest_util_decode_data(strlen(string), string,
					     data_sep - string + 1,
					     &output_len, &output);
		if (!rc) {
			rc = attest_util_write_buf(fd, output, output_len);
			free(output);
		}

		break;
	case DATA_FMT_URI:
		rc = attest_util_download_data(data_sep + 1, fd);
		break;
	default:
		break;
	}

	close(fd);

	if (rc)
		return rc;

	return attest_ctx_data_add_common(ctx, field, data_path_template,
					  0, NULL, label);
}

/**
 * Create new string \<fmt\>:\<data\>
 * @param[in] fmt		data format
 * @param[in] data_len		data length
 * @param[in] data		data
 * @param[in,out] string	created string
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_data_new_string(enum data_formats fmt, size_t data_len,
			       unsigned char *data, char **string)
{
	const char *format_str = data_formats_str[fmt];
	size_t string_len;
	int rc = 0;

	switch (fmt) {
	case DATA_FMT_BASE64:
		rc = attest_util_encode_data(data_len, data,
					     strlen(format_str) + 1,
					     &string_len, string);
		if (rc)
			return rc;
		break;
	case DATA_FMT_URI:
		string_len = strlen(format_str) + 1 + data_len + 1;
		*string = malloc(string_len);
		if (!*string)
			return -ENOMEM;

		memcpy(*string + strlen(format_str) + 1, data, data_len);
		break;
	default:
		break;
	}

	memcpy(*string, format_str, strlen(format_str));
	(*string)[strlen(format_str)] = ':';
	return rc;
}

/**
 * Lookup data item by label
 * @param[in] ctx	data context
 * @param[in] label	data label
 *
 * @returns data_item pointer on success, NULL if not found
 */
struct data_item *attest_ctx_data_lookup_by_label(attest_ctx_data *ctx,
						  const char *label)
{
	struct data_item *item;

	if (!ctx)
		return NULL;

	list_for_each_entry(item, &ctx->ctx_data[CTX_AUX_DATA], list) {
		if (label && !strcmp(item->label, label))
			return item;
	}

	return NULL;
}

/**
 * Lookup data item by digest
 * @param[in] ctx	data context
 * @param[in] algo	digest algorithm
 * @param[in] digest	digest
 *
 * @returns data_item pointer on success, NULL if not found
 */
struct data_item *attest_ctx_data_lookup_by_digest(attest_ctx_data *ctx,
				const char *algo, const uint8_t *digest)
{
	struct data_item *item;
	uint8_t data_digest[MAX_DIGEST_SIZE];
	int rc, digest_len;

	if (!ctx)
		return NULL;

	list_for_each_entry(item, &ctx->ctx_data[CTX_AUX_DATA], list) {
		if (!item->mapped_file)
			continue;

		rc = attest_util_calc_digest(algo, &digest_len, data_digest,
					     item->len, item->data);
		if (rc)
			return NULL;

		if (!memcmp(data_digest, digest, digest_len))
			return item;
	}

	return NULL;
}

/**
 * Return global data context
 *
 * @returns data context
 */
attest_ctx_data *attest_ctx_data_get_global(void)
{
	return &global_ctx_data;
}

/**
 * Obtain and initialize new data context
 * @param[in,out] ctx	data context
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_data_init(attest_ctx_data **ctx)
{
	attest_ctx_data *new_ctx = &global_ctx_data;
	int rc = 0, i;

	if (ctx) {
		new_ctx = calloc(1, sizeof(*new_ctx));
		if (!new_ctx)
			return -ENOMEM;
	}

	for (i = 0; i < CTX__LAST; i++)
		INIT_LIST_HEAD(&new_ctx->ctx_data[i]);

	new_ctx->data_dir = strdup(TEMP_DIR_TEMPLATE);
	if (!new_ctx->data_dir) {
		rc = -ENOMEM;
		goto out;
	}

	new_ctx->data_dir = mkdtemp(new_ctx->data_dir);
	if (!new_ctx->data_dir) {
		rc = -EACCES;
		goto out;
	}

	new_ctx->flags = CTX_INIT;

	if (ctx)
		*ctx = new_ctx;

	return rc;
out:
	free(new_ctx->data_dir);

	if (new_ctx != &global_ctx_data) {
		free(new_ctx);
	}

	return rc;
}

/**
 * Deinitialize data context
 * @param[in] ctx	data context
 */
void attest_ctx_data_cleanup(attest_ctx_data *ctx)
{
	struct data_item *item, *temp_item;
	struct list_head *head;
	int i;

	if (!ctx)
		ctx = &global_ctx_data;

	if (!(ctx->flags & CTX_INIT))
		return;

	for (i = 0; i < CTX__LAST; i++) {
		head = ctx->ctx_data + i;

		list_for_each_entry_safe(item, temp_item, head, list) {
			list_del(&item->list);

			memset(item->data, 0, item->len);

			if (item->mapped_file &&
			    !strncmp(item->mapped_file, ctx->data_dir,
				     strlen(ctx->data_dir))) {
				munmap(item->data, item->len);
				unlink(item->mapped_file);
			} else if (!item->mapped_file) {
				free(item->data);
			}

			free(item->label);
			free(item->mapped_file);
			free(item);
		}
	}

	if (ctx->data_dir) {
		rmdir(ctx->data_dir);
		free(ctx->data_dir);
		ctx->data_dir = NULL;
	}

	memset(ctx, 0, sizeof(*ctx));

	if (ctx != &global_ctx_data)
		free(ctx);
}

/** @} */

/**
 * @name Verifier Context API
 *  @{
 */

/**
 * Get verifier structure
 * @param[in] ctx	verifier context
 * @param[in] id	verifier identifier
 *
 * @returns verifier on success, NULL if not found
 */
struct verifier_struct *attest_ctx_verifier_lookup(attest_ctx_verifier *ctx,
						   const char *id)
{
	struct verifier_struct *verifier;

	if (!ctx)
		return NULL;

	list_for_each_entry(verifier, &ctx->verifiers, list) {
		if (verifier->id == id)
			return verifier;
	}

	return NULL;
}

static int attest_ctx_verifier_add_func(attest_ctx_verifier *ctx,
					const char *id, void *handle,
					verifier_func func, const char *req)
{
	struct verifier_struct *verifier;
	int rc = 0;

	if (attest_ctx_verifier_lookup(ctx, id))
		return 0;

	verifier = malloc(sizeof(*verifier));
	if (!verifier)
		return -ENOMEM;

	verifier->id = id;
	verifier->handle = handle;
	verifier->func = func;
	verifier->req = req ? strdup(req) : NULL;
	if (req && !verifier->req) {
		rc = -ENOMEM;
		goto out;
	}

	list_add_tail(&verifier->list, &ctx->verifiers);
out:
	if (rc)
		free(verifier);

	return rc;
}

/**
 * Add verification requirement
 * @param[in] ctx	verifier context
 * @param[in] verifier_str	verifier identifier
 * @param[in] req	requirement
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_verifier_req_add(attest_ctx_verifier *ctx,
				const char *verifier_str, const char *req)
{
	const char *separator;
	struct verifier_struct *func_array;
	char library_name[MAX_PATH_LENGTH];
	void *handle;
	int rc = 0, i = 0, *num_func;

	if (!ctx)
		return -EINVAL;

	if (!req)
		return -EINVAL;

	separator = strchr(verifier_str, '|');
	if (!separator)
		separator = verifier_str + strlen(verifier_str);

	snprintf(library_name, sizeof(library_name), "libverifier_%.*s.so",
		 (int)(separator - verifier_str), verifier_str);

	handle = dlopen(library_name, RTLD_LAZY);
	if (!handle)
		return -ENOENT;

	num_func = dlsym(handle, "num_func");
	if (!num_func) {
		rc = -ENOENT;
		goto out;
	}

	func_array = dlsym(handle, "func_array");
	if (!func_array) {
		rc = -ENOENT;
		goto out;
	}

	for (i = 0; i < *num_func; i++) {
		if (!strcmp(func_array[i].id, verifier_str))
			break;
	}

	if (i == *num_func) {
		rc = -ENOENT;
		goto out;
	}

	rc = attest_ctx_verifier_add_func(ctx, func_array[i].id, handle,
					  func_array[i].func, req);
out:
	if (rc)
		dlclose(handle);

	return rc;
}

static void attest_ctx_verifier_free_logs(attest_ctx_verifier *ctx)
{
	struct verification_log *log, *temp_log;

	list_for_each_entry_safe(log, temp_log, &ctx->logs, list) {
		list_del(&log->list);
		if (log == &unknown_log)
			break;

		if (strlen(log->reason))
			free(log->reason);

		free(log);
	}
}

/**
 * Create new log
 * @param[in] ctx	verifier context
 * @param[in] operation	operation being performed during verification
 *
 * @returns log on success, NULL on error
 */
struct verification_log *attest_ctx_verifier_add_log(attest_ctx_verifier *ctx,
						     const char *operation)
{
	struct verification_log *new_log, *last_log;

	if (!ctx)
		return NULL;

	last_log = list_last_entry(&ctx->logs, struct verification_log, list);
	if (last_log == &unknown_log)
		return NULL;

	new_log = calloc(1, sizeof(*new_log));
	if (!new_log) {
		attest_ctx_verifier_free_logs(ctx);
		new_log = &unknown_log;
		return NULL;
	}

	new_log->operation = operation;
	new_log->result = "in progress";
	new_log->reason = "";

	list_add(&new_log->list, &ctx->logs);
	return new_log;
}

/**
 * Get current log
 * @param[in] ctx	verifier context
 *
 * @returns log on success, NULL on error
 */
struct verification_log *attest_ctx_verifier_get_log(attest_ctx_verifier *ctx)
{
	struct verification_log *log;

	if (!ctx)
		return NULL;

	if (list_empty(&ctx->logs))
		return NULL;

	list_for_each_entry(log, &ctx->logs, list) {
		if (!strcmp(log->result, "in progress"))
			return log;
	}

	return NULL;
}

/**
 * Set log message
 * @param[in] log	log
 * @param[in] fmt	message format
 * @param[in] ...	data to be added to the message
 */
void attest_ctx_verifier_set_log(struct verification_log *log,
				 const char *fmt, ...)
{
	char buf[MAX_LOG_LENGTH], *reason;
	va_list list;

	if (!log)
		return;

	if (strlen(log->reason))
		return;

	va_start(list, fmt);

	vsnprintf(buf, sizeof(buf), fmt, list);
	reason = strdup(buf);
	if (!reason)
		reason = unknown_log.reason;

	log->reason = reason;
	log->result = "failed";
}

/**
 * Set result in the log
 * @param[in] ctx	verifier context
 * @param[in] log	log
 * @param[in] result	result of the operation performed
 */
void attest_ctx_verifier_end_log(attest_ctx_verifier *ctx,
				 struct verification_log *log, int result)
{
	struct verification_log *previous_log;
	char buf[MAX_LOG_LENGTH];

	if (!ctx)
		return;

	log->result = !result ? "ok" : "failed";

	if (!result)
		return;

	list_for_each_entry_reverse(previous_log, &log->list, list) {
		if ((struct list_head *)previous_log == &ctx->logs)
			break;

		if (strlen(previous_log->reason)) {
			snprintf(buf, sizeof(buf), "%s failed",
				 previous_log->operation);
			if (strlen(log->reason))
				free(log->reason);
			log->reason = strdup(buf);
			if (!log->reason)
				log->reason = unknown_log.reason;

			break;
		}
	}
}

/**
 * Return global verifier context
 *
 * @returns verifier context
 */
attest_ctx_verifier *attest_ctx_verifier_get_global(void)
{
	return &global_ctx_verifier;
}

/**
 * Obtain and initialize verifier context
 * @param[in,out] ctx	verifier context
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_verifier_init(attest_ctx_verifier **ctx)
{
	attest_ctx_verifier *new_ctx = &global_ctx_verifier;

	if (ctx) {
		new_ctx = calloc(1, sizeof(*new_ctx));
		if (!new_ctx)
			return -ENOMEM;
	}

	INIT_LIST_HEAD(&new_ctx->event_logs);
	INIT_LIST_HEAD(&new_ctx->verifiers);
	INIT_LIST_HEAD(&new_ctx->logs);

	new_ctx->flags = CTX_INIT;

	if (ctx)
		*ctx = new_ctx;

	return 0;
}

/**
 * Set random key in the verifier context
 * @param[in] ctx	verifier context
 * @param[in] key_len	HMAC key length
 * @param[in] key	verifier context
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_verifier_set_key(attest_ctx_verifier *ctx, int key_len,
				unsigned char *key)
{
	if (key_len > sizeof(ctx->key))
		return -EINVAL;

	memcpy(ctx->key, key, key_len);
	return 0;
}

/**
 * Set desired PCR mask for key policy verification
 * @param[in] ctx		verifier context
 * @param[in] pcr_mask_len	PCR mask length
 * @param[in] pcr_mask		PCR mask
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_verifier_set_pcr_mask(attest_ctx_verifier *ctx,
				     int pcr_mask_len, uint8_t *pcr_mask)
{
	if (pcr_mask_len > sizeof(ctx->pcr_mask))
		return -EINVAL;

	memset(ctx->pcr_mask, 0, sizeof(ctx->pcr_mask));
	memcpy(ctx->pcr_mask, pcr_mask, pcr_mask_len);
	return 0;
}

/**
 * Set verifier context flags
 * @param[in] ctx		verifier context
 */
void attest_ctx_verifier_set_flags(attest_ctx_verifier *ctx, uint16_t flags)
{
	ctx->flags |= flags;
}

/**
 * Deinitialize verifier context
 * @param[in] ctx	verifier context
 */
void attest_ctx_verifier_cleanup(attest_ctx_verifier *ctx)
{
	struct verifier_struct *v, *temp_v;

	if (!ctx)
		ctx = &global_ctx_verifier;

	if (!(ctx->flags & CTX_INIT))
		return;

	list_for_each_entry_safe(v, temp_v, &ctx->verifiers, list) {
		list_del(&v->list);
		free(v->req);
		free(v);
	}

	attest_ctx_verifier_free_logs(ctx);

	memset(ctx, 0, sizeof(*ctx));

	if (ctx != &global_ctx_verifier)
		free(ctx);
}
/** @}*/
/** @}*/
