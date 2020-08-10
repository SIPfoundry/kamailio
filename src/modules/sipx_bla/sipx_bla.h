/*
 * sipx_bla module - sipx_bla header file
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

#ifndef _SIPX_BLA_H_
#define _SIPX_BLA_H_

#include "../sl/sl.h"
#include "../presence/bind_presence.h"
#include "../pua/pua_bind.h"
#include "../../core/str.h"

extern str db_sipx_im_url;
extern str db_table_entity;
extern str server_address;
extern str outbound_proxy;
extern str bla_header_name;
extern str message_queue_plugin_path;
extern int poll_sipx_bla_user;
extern int poll_sipx_interval;

extern int use_bla_message_queue;
extern str bla_message_queue_log_file;
extern str bla_message_queue_channel;
extern str bla_message_queue_redis_address;
extern str bla_message_queue_redis_password;
extern int bla_message_queue_redis_port;
extern int bla_message_queue_redis_db;


extern pua_api_t pua; /*!< Structure containing pointers to PUA functions*/
extern presence_api_t pres; /*!< Structure containing pointers to PRESENCE functions*/
extern sl_api_t slb; /*!< Structure containing pointers to s functions*/

#endif // ! _SIPX_BLA_H_

