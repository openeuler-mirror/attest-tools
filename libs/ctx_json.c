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
 * File: ctx_json.c
 *      JSON specific context functions.
 */

/**
 * @defgroup context-api-json Context API (JSON)
 * @ingroup context-api
 * @brief
 * JSON specific functions for data and verifier context
 */

/**
 * @addtogroup context-api-json
 *  @{
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "ctx_json.h"
#include "util.h"

/**
 * @name Data Context API
 *  @{
 */

static int attest_ctx_data_add_json(attest_ctx_data *ctx, json_object *obj,
				    enum ctx_fields field,
				    const char *data_label)
{
	struct json_object_iterator v_it, v_itEnd;
	json_object *data_obj;
	enum json_type obj_type;
	const char *key, *label;
	int rc = 0, lookup_field = 0, i;

	obj_type = json_object_get_type(obj);

	switch (obj_type) {
	case json_type_string:
		rc = attest_ctx_data_add_string(ctx, field,
						json_object_get_string(obj),
						data_label);
		break;
	case json_type_array:
		for (i = 0; i < json_object_array_length(obj); i++) {
			data_obj = json_object_array_get_idx(obj, i);
			rc = attest_ctx_data_add_json(ctx, data_obj, field,
						      data_label);
			if (rc)
				return rc;
		}

		break;
	case json_type_object:
		v_it = json_object_iter_begin(obj);
		v_itEnd = json_object_iter_end(obj);
		lookup_field = (field == CTX__LAST);

		while (!json_object_iter_equal(&v_it, &v_itEnd)) {
			label = data_label;
			key = json_object_iter_peek_name(&v_it);

			if (lookup_field) {
				field = attest_ctx_data_lookup_field(key);

				if (field == CTX__LAST)
					return -EINVAL;
			}

			if (field == CTX_EVENT_LOG || field == CTX_AUX_DATA)
				label = key;

			json_object_object_get_ex(obj, key, &data_obj);

			rc = attest_ctx_data_add_json(ctx, data_obj, field,
						      label);
			if (rc)
				return rc;

			json_object_iter_next(&v_it);
		}

		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

/**
 * Parse JSON data
 * @param[in] data	data to parse
 * @param[in] len	length of data to parse
 *
 * @returns JSON object on success, NULL on error
 */
struct json_object *attest_ctx_parse_json_data(const char *data, size_t len)
{
	struct json_tokener *tok;
	enum json_tokener_error e;
	json_object *root;

	tok = json_tokener_new();
	root = json_tokener_parse_ex(tok, (const char *)data, len);

	e = json_tokener_get_error(tok);
	if (e != json_tokener_success)
		printf("JSON parsing error: %s\n", json_tokener_error_desc(e));

	json_tokener_free(tok);
	return root;
}

/**
 * Parse JSON file
 * @param[in] path	file path
 *
 * @returns JSON object on success, NULL on error
 */
struct json_object *attest_ctx_parse_json_file(const char *path)
{
	json_object *root;
	unsigned char *data;
	size_t len;
	int rc;

	rc = attest_util_read_file(path, &len, &data);
	if (rc)
		return NULL;

	root = attest_ctx_parse_json_data((char *)data, len);
	munmap(data, len);
	return root;
}

/**
 * Add JSON data containing data to data context
 * @param[in] ctx	data context
 * @param[in] data	data to parse
 * @param[in] len	length of data to parse
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_data_add_json_data(attest_ctx_data *ctx, const char *data,
				  size_t len)
{
	json_object *root;
	int rc;

	if (!ctx)
		return -EINVAL;

	root = attest_ctx_parse_json_data(data, len);
	if (!root)
		return -EINVAL;

	rc = attest_ctx_data_add_json(ctx, root, CTX__LAST, NULL);
	json_object_put(root);
	return rc;
}

/**
 * Add JSON file containing data to data context
 * @param[in] ctx	data context
 * @param[in] path	file path
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_data_add_json_file(attest_ctx_data *ctx, const char *path)
{
	json_object *root;
	int rc;

	if (!ctx)
		return -EINVAL;

	root = attest_ctx_parse_json_file(path);
	if (!root)
		return -EINVAL;

	rc = attest_ctx_data_add_json(ctx, root, CTX__LAST, NULL);
	json_object_put(root);
	return rc;
}

static int attest_ctx_data_print_json_common(attest_ctx_data *ctx,
					     int display_value, char **json_str)
{
	struct data_item *item;
	json_object *root, *obj, *jstr;
	enum ctx_fields field;
	char *str;
	int rc, j;

	if (!ctx)
		return -EINVAL;

	root = json_object_new_object();
	if (!root)
		return -ENOMEM;

	for (field = 0; field < CTX__LAST; field++) {
		if (list_empty(&ctx->ctx_data[field]))
			continue;

		if (field == CTX_EVENT_LOG || field == CTX_AUX_DATA)
			obj = json_object_new_object();
		else
			obj = json_object_new_array();

		j = 0;

		list_for_each_entry(item, &ctx->ctx_data[field], list) {
			if (display_value) {
				rc = attest_ctx_data_new_string(DATA_FMT_BASE64,
						item->len, item->data, &str);
				if (rc)
					continue;
			} else {
				str = strdup("<value>");
				if (!str)
					continue;
			}

			jstr = json_object_new_string((char *)str);
			if (json_object_get_type(obj) == json_type_array)
				json_object_array_put_idx(obj, j++, jstr);
			else if (item->label)
				json_object_object_add(obj, item->label, jstr);

			free(str);
		}

		json_object_object_add(root, attest_ctx_data_get_field(field),
				       obj);
	}

	*json_str = strdup(json_object_to_json_string_ext(root,
						JSON_C_TO_STRING_PRETTY));

	json_object_put(root);
	return 0;
}

/**
 * Print data context in JSON format
 * @param[in] ctx		data context
 * @param[in,out] json_str	string containing data in JSON format
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_data_print_json(attest_ctx_data *ctx, char **json_str)
{
	return attest_ctx_data_print_json_common(ctx, 1, json_str);
}

/**
 * Print data context in JSON format without values of JSON objects
 * @param[in] ctx		data context
 * @param[in,out] json_str	string containing data in JSON format
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_data_print_json_no_value(attest_ctx_data *ctx, char **json_str)
{
	return attest_ctx_data_print_json_common(ctx, 0, json_str);
}

/**
 * Get data from a JSON string
 * @param[in] json_data		Input data in JSON format
 * @param[in] field		Type of data to extract
 * @param[in,out] data_out_len	Output data length
 * @param[in,out] data_out	Output data
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_data_json_get_by_field(char *json_data, enum ctx_fields field,
				      int *data_out_len,
				      unsigned char **data_out)
{
	attest_ctx_data *d_ctx_in = NULL;
	struct data_item *item;
	int rc = 0;

	attest_ctx_data_init(&d_ctx_in);

	rc = attest_ctx_data_add_json_data(d_ctx_in, json_data,
					   strlen(json_data));
	if (rc < 0)
		goto out;

	item = attest_ctx_data_get(d_ctx_in, field);
	if (!item) {
		rc = -ENOENT;
		goto out;
	}

	*data_out_len = item->len;
	*data_out = malloc(*data_out_len);
	if (!*data_out) {
		rc = -ENOMEM;
		goto out;
	}

	memcpy(*data_out, item->data, item->len);
out:
	attest_ctx_data_cleanup(d_ctx_in);
	return rc;
}
/** @}*/

/**
 * @name Verifier Context API
 *  @{
 */

/**
 * Add JSON file containing requirements to verifier context
 * @param[in] ctx	verifier context
 * @param[in] path	file path
 *
 * @returns 0 on success, a negative value on error
 */
int attest_ctx_verifier_req_add_json_file(attest_ctx_verifier *ctx,
					  const char *path)
{
	struct json_object_iterator l_it, l_itEnd;
	json_object *root, *req_obj, *req;
	const char *verifier_str;
	int rc = -EINVAL;

	if (!ctx)
		return rc;

	root = attest_ctx_parse_json_file(path);
	if (!root)
		return rc;

	json_object_object_get_ex(root, JSON_REQS_OBJECT_KEY, &req_obj);
	if (!req_obj)
		return rc;

	l_it = json_object_iter_begin(req_obj);
	l_itEnd = json_object_iter_end(req_obj);

	while (!json_object_iter_equal(&l_it, &l_itEnd)) {
		verifier_str = json_object_iter_peek_name(&l_it);

		json_object_object_get_ex(req_obj, verifier_str, &req);

		rc = attest_ctx_verifier_req_add(ctx, verifier_str,
					json_object_get_string(req));
		if (rc)
			goto out;

		json_object_iter_next(&l_it);
	}
out:
	json_object_put(root);
	return rc;
}

typedef void (*get_func)(struct list_head *pos, json_object **a,
			 json_object **b, json_object **c);

static void get_verifier(struct list_head *pos, json_object **a,
			 json_object **b, json_object **c)
{
	struct verifier_struct *verifier;

	verifier = list_entry(pos, struct verifier_struct, list);

	*a = json_object_new_string(verifier->id);
	*b = json_object_new_string(verifier->req);
	*c = NULL;
}

static void get_result(struct list_head *pos, json_object **a, json_object **b,
		       json_object **c)
{
	struct verification_log *log;

	log = list_entry(pos, struct verification_log, list);

	*a = json_object_new_string(log->operation);
	*b = json_object_new_string(log->result);
	*c = json_object_new_string(log->reason);
}

static char *attest_ctx_verifier_print_json(struct list_head *head,
					    get_func func)
{
	struct list_head *pos;
	json_object *root, *parent, *obj;
	json_object *a, *b, *c;
	char *output = "";

	root = json_object_new_object();
	if (!root)
		return NULL;

	if (func == &get_verifier)
		parent = json_object_new_object();
	else
		parent = json_object_new_array();

	if (!parent)
		goto out;

	list_for_each(pos, head) {
		func(pos, &a, &b, &c);

		if (func == &get_verifier) {
			json_object_object_add(parent,
					       json_object_get_string(a), b);
			json_object_put(a);
			continue;
		}

		obj = json_object_new_object();
		if (!obj)
			return "";

		json_object_object_add(obj, "operation", a);
		json_object_object_add(obj, "result", b);
		json_object_object_add(obj, "reason", c);
		json_object_array_add(parent, obj);
	}

	json_object_object_add(root, func == &get_verifier ?
			       JSON_REQS_OBJECT_KEY : JSON_LOGS_OBJECT_KEY,
			       parent);

	output = strdup(json_object_to_json_string_ext(root,
						JSON_C_TO_STRING_PRETTY));
out:
	json_object_put(root);
	return output;
}

/**
 * Print loaded requirements in JSON format
 *
 * Returned string must be freed by the caller.
 *
 * @param[in] ctx	verifier context
 *
 * @returns requirement string on success, NULL on error
 */
char *attest_ctx_verifier_req_print_json(attest_ctx_verifier *ctx)
{
	LIST_HEAD(head);

	return attest_ctx_verifier_print_json(ctx ? &ctx->verifiers : &head,
					      get_verifier);
}

/**
 * Print verification logs in JSON format
 *
 * Returned string must be freed by the caller.
 *
 * @param[in] ctx	verifier context
 *
 * @returns verification logs on success, NULL on error
 */
char *attest_ctx_verifier_result_print_json(attest_ctx_verifier *ctx)
{
	LIST_HEAD(head);

	return attest_ctx_verifier_print_json(ctx ? &ctx->logs : &head,
					      get_result);
}
/** @}*/
/** @}*/
