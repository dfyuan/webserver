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

#ifndef CHEROKEE_VALIDATOR_MYSQL_H
#define CHEROKEE_VALIDATOR_MYSQL_H

#include "validator.h"
#include "validator_database_base.h"
#include "connection.h"

#include <mysql.h>

typedef struct {
	cherokee_validator_database_base_t  validator;
	MYSQL		                   *conn;
} cherokee_validator_mysql_t;

typedef struct {
	cherokee_validator_database_base_props_t base;
} cherokee_validator_mysql_props_t;

#define MYSQL(x)           ((cherokee_validator_mysql_t *)(x))
#define PROP_MYSQL(p)      ((cherokee_validator_mysql_props_t *)(p))
#define VAL_MYSQL_PROP(x)  (PROP_MYSQL (MODULE(x)->props))

ret_t cherokee_validator_mysql_configure (cherokee_config_node_t *conf, cherokee_server_t *srv, cherokee_module_props_t **_props);

ret_t cherokee_validator_mysql_new         (cherokee_validator_mysql_t **mysql, cherokee_module_props_t *props);
ret_t cherokee_validator_mysql_free        (cherokee_validator_mysql_t  *mysql);

ret_t cherokee_validator_mysql_query       (cherokee_validator_mysql_t *mysql, cherokee_buffer_t query, cherokee_buffer_t user, cherokee_buffer_t *db_passwd);
ret_t cherokee_validator_mysql_connect     (cherokee_validator_mysql_t *mysql, cherokee_source_t *src_ref);

#endif /* CHEROKEE_VALIDATOR_MYSQL_H */
