/*
 * sipx_bla module - cmd header file
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

#ifndef _SIPX_BLA_CMD_H_
#define _SIPX_BLA_CMD_H_

#include "../../core/parser/msg_parser.h"

int sipx_is_bla_user_cmd(struct sip_msg* msg, char* uri, char* param2);
int sipx_subscribe_bla_users_cmd(struct sip_msg* msg, char* param1, char* param2);
int bla_user_fixup(void** param, int param_no);

#endif // ! _SIPX_BLA_CMD_H_

