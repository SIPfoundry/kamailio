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
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <time.h>

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../parser/parse_expires.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../hashes.h"
#include "../../modules/tm/tm_load.h"
#include "presence_dialoginfo.h"
#include "send_publish.h"

#define MAX_FORWARD 70
#define DEFAULT_DIALOG_EXPIRE 125

extern db_locking_t db_table_lock;

static str default_dialog_content_type = str_init("application/dialog-info+xml");

str* publ_build_hdr(int expires, str* content_type,
		str* extra_headers, int is_body)
{
	static char buf[3000];
	str* str_hdr = NULL;	
	char* expires_s = NULL;
	int len = 0;
	int t= 0;
	str ctype;

	str_hdr =(str*)pkg_malloc(sizeof(str));
	if(str_hdr== NULL)
	{
		LM_ERR("no more memory\n");
		return NULL;
	}
	memset(str_hdr, 0 , sizeof(str));
	memset(buf, 0, 2999);
	str_hdr->s = buf;
	str_hdr->len= 0;

	memcpy(str_hdr->s ,"Max-Forwards: ", 14);
	str_hdr->len = 14;
	str_hdr->len+= sprintf(str_hdr->s+ str_hdr->len,"%d", MAX_FORWARD);
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	memcpy(str_hdr->s+ str_hdr->len ,"Event: dialog", 13);
	str_hdr->len+= 13;
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;
	
	memcpy(str_hdr->s+str_hdr->len ,"Expires: ", 9);
	str_hdr->len += 9;

	t= expires; 

	if( t<=0 )
	{
		t=DEFAULT_DIALOG_EXPIRE;
	}
	else
	{
		t++;
	}
	expires_s = int2str(t, &len);

	memcpy(str_hdr->s+str_hdr->len, expires_s, len);
	str_hdr->len+= len;
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;
	
	if(is_body)
	{
		if(content_type== NULL || content_type->s== NULL || content_type->len== 0)
		{
			ctype = default_dialog_content_type; /* use event default value */ 
		}
		else
		{	
			ctype.s=   content_type->s;
			ctype.len= content_type->len;
		}	
		ctype.s = content_type->s;
		ctype.len = content_type->len;

		memcpy(str_hdr->s+str_hdr->len,"Content-Type: ", 14);
		str_hdr->len += 14;
		memcpy(str_hdr->s+str_hdr->len, ctype.s, ctype.len);
		str_hdr->len += ctype.len;
		memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
		str_hdr->len += CRLF_LEN;
	}

	if(extra_headers && extra_headers->s && extra_headers->len)
	{
		memcpy(str_hdr->s+str_hdr->len,extra_headers->s , extra_headers->len);
		str_hdr->len += extra_headers->len;
	}	
	str_hdr->s[str_hdr->len] = '\0';
	
	return str_hdr;

}

void publ_cback_func(struct cell *t, int type, struct tmcb_params *ps)
{
	//TODO:
	//Do we handle publish response if succeed/failed?
	return;
}	

int send_publish( publ_info_t* publ )
{
	str met = {"PUBLISH", 7};
	str* str_hdr = NULL;
	int result;
	uac_req_t uac_r;
	int ret = -1;

	LM_DBG("pres_uri=%.*s\n", publ->pres_uri->len, publ->pres_uri->s );
	
	if(publ->body== NULL)
	{
		LM_ERR("New PUBLISH and no body found- invalid request\n");
		ret = ERR_PUBLISH_NO_BODY;
		goto finish;
	}

	str_hdr = publ_build_hdr((publ->expires< 0)?DEFAULT_DIALOG_EXPIRE:publ->expires, &publ->content_type, 
				publ->extra_headers, 1);

	if(str_hdr == NULL)
	{
		LM_ERR("while building extra_headers\n");
		goto finish;
	}

	LM_DBG("publ->pres_uri:\n%.*s\n ", publ->pres_uri->len, publ->pres_uri->s);
	LM_DBG("str_hdr:\n%.*s %d\n ", str_hdr->len, str_hdr->s, str_hdr->len);
	if(publ->body && publ->body->len && publ->body->s )
		LM_DBG("body:\n%.*s\n ", publ->body->len, publ->body->s);

	set_uac_req(&uac_r, &met, str_hdr, publ->body, 0, TMCB_LOCAL_COMPLETED,
			publ_cback_func, NULL);
	result= tmb.t_request(&uac_r,
			publ->pres_uri,			/*! Request-URI */
			publ->pres_uri,			/*! To */
		    publ->pres_uri,			/*! From */
		    publ->outbound_proxy?publ->outbound_proxy:&server_address /*! Outbound proxy*/
			);

	if(result< 0)
	{
		LM_ERR("in t_request tm module function\n");
		goto finish;
	}

	ret = 0;
finish:
	if(str_hdr)
		pkg_free(str_hdr);

	return ret;
}
