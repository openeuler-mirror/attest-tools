/*
 * Copyright (C) 2018-2021 Huawei Technologies Duesseldorf GmbH
 *
 * Author: Roberto Sassu <roberto.sassu@huawei.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * File: ima_sig.c
 *      Verifier of IMA signatures.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <digestlist/crypto.h>
#include "ctx.h"
#include "util.h"
#include "event_log/ima.h"

#define IMA_SIG_ID "ima_sig|verify"
#define IMA_CERT_ID "x509_ima.der"

enum req_types { REQ_ISSUER, REQ_SUBJECT, REQ_SUBJECT_ID, REQ__LAST };

char *requirements[REQ__LAST] = {
	[REQ_ISSUER] = "issuer:",
	[REQ_SUBJECT] = "subject:",
	[REQ_SUBJECT_ID] = "subject_id:",
};

struct req_struct {
	struct list_head list;
	enum req_types type;
	char *req;
};

static void free_reqs(struct list_head *head)
{
	struct req_struct *p, *q;

	list_for_each_entry_safe(p,q, head, list) {
		list_del(&p->list);
		free(p);
	}
}

int verify(attest_ctx_data *d_ctx, attest_ctx_verifier *v_ctx)
{
	struct data_item *ima_cert_item, *item;
	struct event_log_entry *cur_log_entry, *key_entry = NULL;
	struct ima_log_entry *ima_log_entry;
	struct event_log *ima_log;
	struct verifier_struct *verifier;
	struct verification_log *log;
	struct key_struct *key;
	struct req_struct *req_struct, *new_req;
	LIST_HEAD(head);
	LIST_HEAD(req_head);
	enum hash_algo algo;
	const u8 *sig_ptr, *digest_ptr;
	const char *algo_ptr, *eventname_ptr;
	u32 sig_len, digest_len, algo_len, eventname_len;
	X509 *cert = NULL;
	X509_NAME *name = NULL;
	ASN1_OCTET_STRING *skid = NULL;
	const unsigned char *ptr, *skid_str;
	char issuer[256], subject[256], keyid[9] = { 0 };
	char *req_copy = NULL, *req_copy_ptr, *req;
	int rc = 0, req_found = 0, i, skid_len;

	log = attest_ctx_verifier_add_log(v_ctx, "verify IMA signatures");

	verifier = attest_ctx_verifier_lookup(v_ctx, IMA_SIG_ID);

	check_goto(!verifier->req, -ENOENT, out, v_ctx,
		   "requirement not provided");

	req_copy_ptr = req_copy = strdup(verifier->req);
	check_goto(!req_copy, -ENOMEM, out, v_ctx, "out of memory");

	while ((req = strsep(&req_copy_ptr, ","))) {
		for (i = 0; i < REQ__LAST; i++) {
			if (!strncmp(req, requirements[i],
			    strlen(requirements[i])))
				break;
		}

		check_goto(i == REQ__LAST, -EINVAL, out, v_ctx,
			   "invalid requirement: %s", req);

		new_req = malloc(sizeof(*new_req));
		check_goto(!new_req, -ENOMEM, out, v_ctx, "out of memory");
		new_req->type = i;
		new_req->req = req + strlen(requirements[i]);
		list_add(&new_req->list, &req_head);
	}

	ima_log = attest_event_log_get(v_ctx, "ima");
	check_goto(!ima_log, -ENOENT, out, v_ctx,
		   "IMA event log not provided");

	ima_cert_item = ima_lookup_data_item(d_ctx, ima_log, IMA_CERT_ID,
					     &key_entry);
	if (ima_cert_item) {
		ptr = ima_cert_item->data;
		cert = d2i_X509(NULL, &ptr, ima_cert_item->len);

		check_goto(!cert, -ENOENT, out, v_ctx,
			   "IMA certificate cannot be parsed");

		name = X509_get_issuer_name(cert);
		check_goto(!name, -ENOENT, out, v_ctx,
			   "IMA certificate cannot be parsed");
		X509_NAME_oneline(name, issuer, sizeof(issuer));

		name = X509_get_subject_name(cert);
		check_goto(!name, -ENOENT, out, v_ctx,
			   "IMA certificate cannot be parsed");
		X509_NAME_oneline(name, subject, sizeof(subject));

		req_found = 0;
		list_for_each_entry(req_struct, &req_head, list) {
			switch (req_struct->type) {
			case REQ_ISSUER:
				if (!strcmp(issuer, req_struct->req))
					req_found = 1;
				break;
			case REQ_SUBJECT:
				if (!strcmp(subject, req_struct->req))
					req_found = 1;
				break;
			case REQ_SUBJECT_ID:
				skid = X509_get_ext_d2i(cert,
					NID_subject_key_identifier, NULL, NULL);
				if (!skid)
					break;

				skid_str = (u8 *)ASN1_STRING_get0_data(skid);
				skid_len = ASN1_STRING_length(skid);

				_bin2hex(keyid, skid_str + skid_len - 4, 4);

				ASN1_OCTET_STRING_free(skid);

				if (!strcmp(keyid, req_struct->req))
					req_found = 1;
				break;
			default:
				break;
			}
		}

		if (req_found) {
			key = new_key(&head, -1, ima_cert_item->mapped_file,
				      NULL, false);
			check_goto(!key, -ENOENT, out, v_ctx,
				   "IMA public key cannot be retrieved");

			key_entry->flags |= LOG_ENTRY_PROCESSED;
		} else {
			printf("Warning: not adding key %s to the keyring, "
			       "requirements not satisfied\n",
			       ima_cert_item->label);
		}
	}

	list_for_each_entry(item, &d_ctx->ctx_data[CTX_AUX_DATA], list) {
		if (!item->mapped_file)
			continue;

		if (strncmp(item->label, "pgp-key", 7))
			continue;

		key = new_key_pgp(&head, -1, item->mapped_file);
		check_goto(!key, -ENOENT, out, v_ctx, "key cannot be imported");

		_bin2hex(keyid, key->keyid, 4);

		req_found = 0;
		list_for_each_entry(req_struct, &req_head, list) {
			switch (req_struct->type) {
			case REQ_SUBJECT_ID:
				if (!strcmp(keyid, req_struct->req))
					req_found = 1;
				break;
			default:
				break;
			}
		}

		if (!req_found) {
			printf("Warning: not adding key %s to the keyring, "
			       "requirements not satisfied\n", item->label);
			free_key(&head, key);
		}
	}

        list_for_each_entry(cur_log_entry, &ima_log->logs, list) {
		ima_log_entry = (struct ima_log_entry *)cur_log_entry->log;

		rc = ima_template_get_digest(ima_log_entry, &algo_len,
					     &algo_ptr, &digest_len, &digest_ptr);
		check_goto(rc, rc, out, v_ctx, "event digest not found");

		rc = ima_template_get_eventname(ima_log_entry, &eventname_len,
						&eventname_ptr);
		check_goto(rc, rc, out, v_ctx, "event name not found");

		if (!strcmp(eventname_ptr, "boot_aggregate"))
			continue;

		for (algo = 0; algo < HASH_ALGO__LAST; algo++)
			if (!strncmp(hash_algo_name[algo], algo_ptr, algo_len))
				break;

		check_goto(algo == HASH_ALGO__LAST, -ENOENT, out, v_ctx,
			   "Unknown hash algorithm");

		rc = ima_template_get_field(ima_log_entry, FIELD_SIG, &sig_len,
					    &sig_ptr);
		if (rc < 0 || ! sig_len)
			continue;

		rc = verify_sig(&head, -1, (u8 *)sig_ptr, sig_len,
				(u8 *)digest_ptr, algo);
		check_goto(rc, rc, out, v_ctx, "invalid signature");

		cur_log_entry->flags |= LOG_ENTRY_PROCESSED;
	}
out:
	X509_free(cert);
	free_keys(&head);
	free(req_copy);
	free_reqs(&req_head);
	attest_ctx_verifier_end_log(v_ctx, log, rc);
	return rc;
}

int num_func = 1;

struct verifier_struct func_array[1] = {{.id = IMA_SIG_ID, .func = verify}};
