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
#include "validator_mysql.h"

#include "connection.h"
#include "connection-protected.h"
#include "plugin_loader.h"
#include "util.h"

#define ENTRIES "validator,mysql"
#define MYSQL_DEFAULT_PORT 3306


/* Plug-in initialization
 */
PLUGIN_INFO_VALIDATOR_EASIEST_INIT (mysql, http_auth_basic | http_auth_digest);


ret_t
cherokee_validator_mysql_configure (cherokee_config_node_t *conf, cherokee_server_t *srv, cherokee_module_props_t **_props)
{
	ret_t                             ret;
	cherokee_list_t			 *i;
	cherokee_validator_mysql_props_t *props;

	UNUSED(srv);

	if(*_props == NULL) {
		CHEROKEE_NEW_STRUCT (n, validator_mysql_props);
	
		cherokee_validator_props_init_base (VALIDATOR_PROPS (n), MODULE_PROPS_FREE(cherokee_validator_database_base_props_free));

		*_props = MODULE_PROPS (n);
	}

	props = PROP_MYSQL(*_props);
        /* Init base class
	 */
        ret = cherokee_validator_database_base_configure (conf, srv, _props);
        if (ret != ret_ok) return ret;

	return ret_ok;
}


ret_t
cherokee_validator_mysql_new (cherokee_validator_mysql_t **mysql, cherokee_module_props_t *props)
{
	ret_t ret;
	CHEROKEE_NEW_STRUCT (n, validator_mysql);

	/* Init the base class
	 */
	cherokee_validator_init_base (VALIDATOR(n), VALIDATOR_PROPS(props), PLUGIN_INFO_VALIDATOR_PTR(mysql));

	VALIDATOR(n)->support = http_auth_basic | http_auth_digest;

	MODULE(n)->free           = (module_func_free_t)                cherokee_validator_mysql_free;
	VALIDATOR(n)->check       = (validator_func_check_t)            cherokee_validator_database_base_check;
	VALIDATOR(n)->add_headers = (validator_func_add_headers_t)      cherokee_validator_database_base_add_headers;
	DATABASE_BASE(n)->connect = (validator_database_base_connect_t) cherokee_validator_mysql_connect; 
	DATABASE_BASE(n)->query   = (validator_database_base_query_t)   cherokee_validator_mysql_query; 

	DATABASE_BASE(n)->src_ref = NULL;

	/* Initialization
	 */
	ret = cherokee_validator_database_base_init_connection (DATABASE_BASE(n), PROP_DATABASE_BASE(props));
	if (ret != ret_ok) {
		cherokee_validator_free (VALIDATOR(n));
		return ret;
	}

	/* Return obj
	 */
	*mysql = n;
	return ret_ok;
}


ret_t
cherokee_validator_mysql_free (cherokee_validator_mysql_t *mysql)
{
	if (mysql->conn != NULL) {
		mysql_close (mysql->conn);
	}

	return ret_ok;
}



ret_t
cherokee_validator_mysql_connect (cherokee_validator_mysql_t *mysql, cherokee_source_t *src_ref)
{
	cherokee_validator_database_base_props_t *props = VAL_DATABASE_BASE_PROP(mysql);
	MYSQL *conn;

	mysql->conn = mysql_init (NULL);
	if (mysql->conn == NULL)
		return ret_nomem;

	conn = mysql_real_connect (mysql->conn,
				   src_ref->host.buf,
				   props->user.buf,
				   props->passwd.buf,
				   props->database.buf,
				   src_ref->port,
				   src_ref->unix_socket.buf, 0);
	
	/* TODO: this code below is broken it should actually display host.buf or unix_socket.buf */
	if (conn == NULL) {
		LOG_ERROR (CHEROKEE_ERROR_VALIDATOR_MYSQL_NOCONN,
			   src_ref->host.buf, src_ref->port, mysql_error (mysql->conn));
		return ret_error;
	}

	TRACE (ENTRIES, "Connected to (%s:%d)\n", src_ref->host.buf, src_ref->port);
	return ret_ok;
}


ret_t
cherokee_validator_mysql_query (cherokee_validator_mysql_t *mysql, cherokee_buffer_t query, cherokee_buffer_t user, cherokee_buffer_t *db_passwd)
{
	int                               re;
	ret_t                             ret;
	MYSQL_ROW	                  row;
	MYSQL_RES	                 *result;
	unsigned long                    *lengths;

	/* Execute query
	 */
	re = mysql_query (mysql->conn, query.buf);
	if (re != 0) {
		return ret_error;
	}

	result = mysql_store_result (mysql->conn);

	re = mysql_num_rows (result);
	if (re <= 0) {
		TRACE (ENTRIES, "User %s was not found\n", user.buf);
		ret = ret_not_found;
		goto done;

	} else if  (re > 1) {
		TRACE (ENTRIES, "The user %s is not unique in the DB\n", user.buf);
		ret = ret_deny;
		goto done;
	}

	/* Copy the user information
	 */
	row     = mysql_fetch_row (result);
	lengths = mysql_fetch_lengths (result);

	cherokee_buffer_add (db_passwd, row[0], (size_t) lengths[0]);
	ret = ret_ok;

done:
	mysql_free_result (result);
	return ret;
}
