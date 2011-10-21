/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2011 Alvaro Lopez Ortega
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "common-internal.h"
#include "validator_database_base.h"

#include "connection.h"
#include "connection-protected.h"
#include "plugin_loader.h"
#include "util.h"

#define ENTRIES "validator,databasebase"
#define CHEROKEE_ERROR_VALIDATOR_DATABASE_KEY 1000
#define CHEROKEE_ERROR_VALIDATOR_DATABASE_HASH 1001
#define CHEROKEE_ERROR_VALIDATOR_DATABASE_NO_BALANCER 1002
#define CHEROKEE_ERROR_VALIDATOR_DATABASE_QUERY 1003


ret_t
cherokee_validator_database_base_props_free (cherokee_validator_database_base_props_t *props)
{
	if (props->balancer)
		cherokee_balancer_free (props->balancer);
	
	cherokee_buffer_mrproper (&props->user);
	cherokee_buffer_mrproper (&props->passwd);
	cherokee_buffer_mrproper (&props->database);
	cherokee_buffer_mrproper (&props->query);

	return cherokee_validator_props_free_base (VALIDATOR_PROPS(props));
}


ret_t
cherokee_validator_database_base_configure (cherokee_config_node_t *conf, cherokee_server_t *srv, cherokee_module_props_t **_props)
{
	ret_t                                     ret;
	cherokee_list_t			         *i;
	cherokee_validator_database_base_props_t *props;

	UNUSED(srv);

	/* Sanity check: This class is pure virtual,
	 * it shouldn't allocate memory here.
	 * Check that the object space has been already instanced by its father.
	 */
	if(*_props == NULL) {
		SHOULDNT_HAPPEN;
		return ret_ok;
	}

	/* Init
	 */
	props = PROP_DATABASE_BASE(*_props);
	props->balancer = NULL;
	
	cherokee_buffer_init (&props->user);
	cherokee_buffer_init (&props->passwd);
	cherokee_buffer_init (&props->database);
	cherokee_buffer_init (&props->query);

	props->hash_type = cherokee_database_hash_none;

	cherokee_config_node_foreach (i, conf) {
		cherokee_config_node_t *subconf = CONFIG_NODE(i);

		if (equal_buf_str (&subconf->key, "balancer")) {
			ret = cherokee_balancer_instance (&subconf->val, subconf, srv, &props->balancer);
			if (ret != ret_ok) return ret;

		} else if (equal_buf_str (&subconf->key, "user")) {
			cherokee_buffer_add_buffer (&props->user, &subconf->val);

		} else if (equal_buf_str (&subconf->key, "passwd")) {
			cherokee_buffer_add_buffer (&props->passwd, &subconf->val);

		} else if (equal_buf_str (&subconf->key, "database")) {
			cherokee_buffer_add_buffer (&props->database, &subconf->val);

		} else if (equal_buf_str (&subconf->key, "query")) {
			cherokee_buffer_add_buffer (&props->query, &subconf->val);

		} else if (equal_buf_str (&subconf->key, "hash")) {
			if (equal_buf_str (&subconf->val, "md5")) {
				props->hash_type = cherokee_database_hash_md5;

			} else if (equal_buf_str (&subconf->val, "sha1")) {
				props->hash_type = cherokee_database_hash_sha1;

			} else if (equal_buf_str (&subconf->val, "sha512")) {
				props->hash_type = cherokee_database_hash_sha512;

			} else {
				LOG_CRITICAL (CHEROKEE_ERROR_VALIDATOR_DATABASE_HASH, subconf->val.buf);
				return ret_error;
			}

		} else if (equal_buf_str (&subconf->key, "methods") ||
			   equal_buf_str (&subconf->key, "realm")   ||
			   equal_buf_str (&subconf->key, "users")) {
			/* Handled in validator.c
			 */
		} else {
			LOG_CRITICAL (CHEROKEE_ERROR_VALIDATOR_DATABASE_KEY, subconf->key.buf);
			return ret_error;
		}
	}

	/* Checks
	 */
	if (props->balancer == NULL) {
		LOG_CRITICAL_S (CHEROKEE_ERROR_VALIDATOR_DATABASE_NO_BALANCER);
		return ret_error;
	}

	if (cherokee_buffer_is_empty (&props->query)) {
		LOG_ERROR_S (CHEROKEE_ERROR_VALIDATOR_DATABASE_QUERY);
		return ret_error;
	}

	return ret_ok;
}

ret_t
cherokee_validator_database_base_init_connection (cherokee_validator_database_base_t       *base,
                                                  cherokee_validator_database_base_props_t *props)
{
	ret_t                  ret;
	cherokee_connection_t *conn = HANDLER_CONN(base);

	/* Get a reference to the target database
	 */
	if (base->src_ref == NULL) {
		ret = cherokee_balancer_dispatch (props->balancer, conn, &base->src_ref);
		if (ret != ret_ok)
			return ret;
	}

	/* Try to connect
	 */
	if (base->src_ref->type == source_host) {
		ret = base->connect(base, base->src_ref);
		if (ret != ret_ok) {
			cherokee_balancer_report_fail (props->balancer, conn, base->src_ref);
		}
	} else {
		/* TODO: Must handle this case... */
		ret = ret_error;
	}

	return ret;
}

ret_t
cherokee_validator_database_base_check (cherokee_validator_database_base_t *base, cherokee_connection_t *conn)
{
	int                                       re;
	ret_t                                     ret;
	cherokee_buffer_t                         db_passwd   = CHEROKEE_BUF_INIT;
	cherokee_buffer_t                         user_passwd = CHEROKEE_BUF_INIT;
	cherokee_buffer_t                         query       = CHEROKEE_BUF_INIT;
	cherokee_validator_database_base_props_t *props	      = VAL_DATABASE_BASE_PROP(base);

	/* Sanity checks
	 */
	if (unlikely ((conn->validator == NULL) ||
		      cherokee_buffer_is_empty (&conn->validator->user))) {
		return ret_error;
	}

	if (unlikely ((strcasestr (conn->validator->user.buf, " or ") != NULL) ||
		      (strcasestr (conn->validator->user.buf, " or/") != NULL) ||
		      (strcasestr (conn->validator->user.buf, "/or ") != NULL)))
	{
		return ret_error;
	}

	re = cherokee_buffer_cnt_cspn (&conn->validator->user, 0, "'\";");
	if (unlikely (re != (int) conn->validator->user.len)) {
		return ret_error;
	}

	/* Build query
	 */
	cherokee_buffer_add_buffer (&query, &props->query);
	cherokee_buffer_replace_string (&query, "${user}", 7, conn->validator->user.buf, conn->validator->user.len);
	cherokee_buffer_replace_string (&query, "${passwd}", 9, conn->validator->passwd.buf, conn->validator->passwd.len);

	TRACE (ENTRIES, "Query: %s\n", query.buf);

	/* Query & Copy the user information
	 */
	ret = base->query(base, query, conn->validator->user, &db_passwd);
	if (ret != ret_ok) {
		goto error;
	}

	/* Check it out
	 */
	switch (conn->req_auth_type) {
	case http_auth_basic:
		cherokee_buffer_add_buffer (&user_passwd, &conn->validator->passwd);

		/* Hashes */
		if (props->hash_type == cherokee_database_hash_md5) {
			cherokee_buffer_encode_md5_digest (&user_passwd);
		} else if (props->hash_type == cherokee_database_hash_sha1) {
			cherokee_buffer_encode_sha1_digest (&user_passwd);
		} else if (props->hash_type == cherokee_database_hash_sha512) {
			cherokee_buffer_encode_sha512_digest (&user_passwd);
		}

		/* Compare passwords */
		re = cherokee_buffer_case_cmp_buf (&user_passwd, &db_passwd);
		ret = (re == 0) ? ret_ok : ret_deny;
		break;

	case http_auth_digest:
		ret = cherokee_validator_digest_check (VALIDATOR(base), &db_passwd, conn);
		break;

	default:
		SHOULDNT_HAPPEN;
		ret = ret_error;
		goto error;
	}

	if (ret != ret_ok) {
		TRACE (ENTRIES, "User %s did not properly authenticate.\n", conn->validator->user.buf);
		ret = ret_not_found;
		goto error;
	}

	TRACE (ENTRIES, "Access to user %s has been granted\n", conn->validator->user.buf);

	/* Clean-up
	 */
	cherokee_buffer_mrproper (&query);
	cherokee_buffer_mrproper (&db_passwd);
	cherokee_buffer_mrproper (&user_passwd);

	return ret_ok;

error:
	cherokee_buffer_mrproper (&query);
	cherokee_buffer_mrproper (&db_passwd);
	cherokee_buffer_mrproper (&user_passwd);
	return ret;
}


ret_t
cherokee_validator_database_base_add_headers (cherokee_validator_database_base_t *base,
				              cherokee_connection_t              *conn,
				              cherokee_buffer_t                  *buf)
{
	UNUSED (base);
	UNUSED (conn);
	UNUSED (buf);

	return ret_ok;
}

