/*
 * presence_dialoginfo module - presence dialoginfo module
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


#ifndef _PRESENCE_SEND_PUBLISH_
#define _PRESENCE_SEND_PUBLISH_
#include <time.h>
#include "../../modules/tm/tm_load.h"
#include "../../str.h"
#define ERR_PUBLISH_NO_BODY -10

typedef struct publ_info
{
	str* pres_uri;
	str* body;
	int expires;
	str content_type;  /*! the content_type of the body if present(optional if the
				same as the default value for that event) */
	str* outbound_proxy;
	str* extra_headers;
	void* cb_param;   /*! the parameter for the function to be called on the callback 
				for the received reply; it must be allocated in share memory;
				a reference to it will be found in the cb_param filed of the ua_pres_structure
				receied as a parameter for the registered function*/
} publ_info_t;

int send_publish( publ_info_t* publ );

void publ_cback_func(struct cell *t, int type, struct tmcb_params *ps);
str* publ_build_hdr(int expires, str* content_type, str* extra_headers, int is_body);

#endif
