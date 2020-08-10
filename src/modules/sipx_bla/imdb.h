/*
 * sipx_bla module - imdb header file
 *
 * Copyright © 2015 SIPfoundry Inc.
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifndef _SIPX_IMDB_H_
#define _SIPX_IMDB_H_
		
#include "../rls/list.h"
#include "../../core/str.h"
#include "../../lib/srdb1/db.h"

int im_db_bind(const str* db_url);
int im_db_init(const str* db_url);
int im_db_init2(const str* db_url);
void im_db_close(void);

int is_user_supports_bla(str* user);
int get_all_bla_users(list_entry_t** users);


extern db_func_t im_dbf;
extern db1_con_t *im_db;

#endif /* _SIPX_IMDB_H_ */
