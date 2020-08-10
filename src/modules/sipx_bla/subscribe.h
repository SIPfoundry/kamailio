/*
 * sipx_bla module - subscribe header file
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

#ifndef _SIPX_SUBSCRIBE_H_
#define _SIPX_SUBSCRIBE_H_

#include "../../core/str.h"
#include "../rls/list.h"

int sipx_reginfo_subscribe(str user);
int sipx_reginfo_subscribe_with_expires(str user, int expires);
int sipx_subscribe_bla_users();

list_entry_t * get_sipx_reginfo_subscribes(str user);


#endif // ! SIPX_SUBSCRIBE_H_
