/*
 * presence module - presence dialoginfo module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 */


#ifndef _PRESENCE_SEND_SUBSC_
#define _PRESENCE_SEND_SUBSC_

#include <time.h>

#include "../../modules/tm/tm_load.h"
#include "../../str.h"

typedef struct subs_info
{
	str* pres_uri;
	str* watcher_uri;
	str* contact;
	str* remote_target;
	str* outbound_proxy;
	str* extra_headers;
	int expires;
} subs_info_t;

int send_subscribe(subs_info_t* subs);
int send_subscribe0(str user, str domain);
void subs_cback_func(struct cell *t, int type, struct tmcb_params *ps);
str* subs_build_hdr(str* watcher_uri, int expires, str* extra_headers);

int init_dialog_timers(void);
void clean_dialog_timers(void);

#endif
