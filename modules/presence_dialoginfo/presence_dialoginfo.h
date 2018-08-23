/*
 * presence_dialoginfo module - presence_dialoginfo header file
 *
 * Copyright (C) 2007 Juha Heinanen
 * Copyright (C) 2008 Klaus Darilion IPCom
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
 * History:
 * --------
 *  2008-08-25  initial version (kd)
 */

#ifndef _PRES_DLGINFO_H_
#define _PRES_DLGINFO_H_

#include "../../modules/tm/tm_load.h"
#include "../presence/presence.h"
#include "dialog_collator.h"

/* DB module bind */
extern db_func_t pa_dbf;
extern db1_con_t* pa_db;

extern struct tm_binds tmb;

/* Hybrid sipx dialog */
extern int enable_dialog_check;
extern int enable_dialog_collate;
extern int dialog_check_period;
extern int dialog_collate_period;
extern str outbound_proxy;
extern str server_address;
extern str default_subscribe_username;

/* Support for polycom aggregation */
extern int force_single_dialog;
extern int force_dummy_dialog;
extern int use_dialog_event_collator;
extern void* collate_handle;
extern collate_plugin_t collate_plugin;

extern add_event_t pres_add_event;

#endif
