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

#ifndef CHEROKEE_VALIDATOR_DATABASE_BASE_H
#define CHEROKEE_VALIDATOR_DATABASE_BASE_H

#include "validator.h"
#include "connection.h"
#include "balancer.h"

typedef ret_t (* validator_database_base_query_t)   (void *, cherokee_buffer_t, cherokee_buffer_t, cherokee_buffer_t *);
typedef ret_t (* validator_database_base_connect_t) (void *, cherokee_source_t *);

typedef struct {
	cherokee_validator_t              validator;
	cherokee_source_t                *src_ref;
	validator_database_base_connect_t connect;
	validator_database_base_query_t   query;
} cherokee_validator_database_base_t;

typedef enum {
	cherokee_database_hash_none,
	cherokee_database_hash_md5,
	cherokee_database_hash_sha1,
	cherokee_database_hash_sha512
} cherokee_database_base_hash_t;

typedef struct {
	cherokee_module_props_t base;

	cherokee_balancer_t    *balancer;

	cherokee_buffer_t       user;
	cherokee_buffer_t       passwd;
	cherokee_buffer_t       database;
	cherokee_buffer_t       query;

	cherokee_database_base_hash_t hash_type;
} cherokee_validator_database_base_props_t;

#define DATABASE_BASE(x)           ((cherokee_validator_database_base_t *)(x))
#define PROP_DATABASE_BASE(p)      ((cherokee_validator_database_base_props_t *)(p))
#define VAL_DATABASE_BASE_PROP(x)  (PROP_DATABASE_BASE (MODULE(x)->props))

ret_t cherokee_validator_database_base_configure (cherokee_config_node_t *conf, cherokee_server_t *srv, cherokee_module_props_t **_props);

ret_t cherokee_validator_database_base_new             (cherokee_validator_database_base_t **base, cherokee_module_props_t *props);
ret_t cherokee_validator_database_base_free            (cherokee_validator_database_base_t  *base);
ret_t cherokee_validator_database_base_check           (cherokee_validator_database_base_t  *base, cherokee_connection_t *conn);
ret_t cherokee_validator_database_base_add_headers     (cherokee_validator_database_base_t  *base, cherokee_connection_t *conn, cherokee_buffer_t *buf);
ret_t cherokee_validator_database_base_init_connection (cherokee_validator_database_base_t  *base, cherokee_validator_database_base_props_t *props);

ret_t cherokee_validator_database_base_props_free (cherokee_validator_database_base_props_t *props);

#endif /* CHEROKEE_VALIDATOR_DATABASE_BASE_H */
