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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libxml/parser.h>

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/tm/dlg.h"
#include "../../parser/msg_parser.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_expires.h"
#include "../../modules/tm/tm_load.h"
#include "../../hashes.h"
#include "../../str_list.h"
#include "../presence/utils_func.h"
#include "send_subscribe.h"
#include "send_publish.h"
#include "presence_dialoginfo.h"

#define MAX_FORWARD 70

/* Custom data structures */
typedef struct pres_uri_list {
	str user;
	str domain;
	str pres_uri;
	struct pres_uri_list * next;
} pres_uri_list_t;

static struct timer_ln* dc_timer = NULL;
static str str_to_user_col = str_init("to_user");
static str str_to_domain_col = str_init("to_domain");
static str str_presentity_uri_col = str_init("presentity_uri");


/* Private methods */
void dialog_active_check_handler(unsigned int ticks,void *param);
ticks_t dialog_active_collate_handler(ticks_t ticks, struct timer_ln* tl, void *p);
pres_uri_list_t* get_active_dialog();
pres_uri_list_t* new_active_dialog(str pres_user, str pres_domain, str pres_uri);
int add_active_dialog(str pres_user, str pres_domain, str pres_uri, pres_uri_list_t* pres_list);
pres_uri_list_t* search_active_dialog(pres_uri_list_t* pres_list, str pres_user, str pres_domain);
void free_pres_uri_list(pres_uri_list_t * pres_list);

subs_info_t* subscribe_cbparam(subs_info_t* subs);

int send_subscribe0(str user, str domain)
{
	subs_info_t subs;
	str pres_uri;
	str watcher_uri;
	int ret = 0;

	if(uandd_to_uri(user, domain, &pres_uri) < 0)
	{
		LM_ERR("while creating pres uri\n");
		return -1;
	}

	if(uandd_to_uri(default_subscribe_username, domain, &watcher_uri) < 0)
	{
		LM_ERR("while creating watcher uri\n");
		ret = -1;
		goto error;
	}

	memset(&subs, 0, sizeof(subs_info_t));
	subs.pres_uri = &pres_uri;
	subs.watcher_uri = &watcher_uri;
	subs.contact = &server_address;
	subs.expires = 0;

	if(send_subscribe(&subs) < 0) 
	{
		LM_ERR("while sending subscribe\n");
		ret = -1;
		goto error;
	}

error:
	if(pres_uri.s)
		pkg_free(pres_uri.s);

	if(watcher_uri.s)
		pkg_free(watcher_uri.s);

	return ret;
}

int send_subscribe(subs_info_t* subs)
{
	str met= {"SUBSCRIBE", 9};
	str* str_hdr= NULL;
	int ret= -1;
	int result;
	uac_req_t uac_r;
	subs_info_t* hentity;

	str_hdr= subs_build_hdr(subs->contact, subs->expires, subs->extra_headers);
	if(str_hdr== NULL || str_hdr->s== NULL)
	{
		LM_ERR("while building extra headers\n");
		return -1;
	}

	hentity= subscribe_cbparam(subs);
	if(hentity== NULL)
	{
		LM_ERR("while building callback param\n");
		goto error;
	}

	set_uac_req(&uac_r, &met, str_hdr, 0, 0, TMCB_LOCAL_COMPLETED,
			subs_cback_func, (void*)hentity);
	result= tmb.t_request_outside
		(&uac_r,					  /* Type of the message */
		subs->remote_target?subs->remote_target:subs->pres_uri,/* Request-URI*/
		subs->pres_uri,				  /* To */
		subs->watcher_uri,			  /* From */
		subs->outbound_proxy?subs->outbound_proxy:&outbound_proxy/* Outbound_proxy */	
		);
	if(result< 0)
	{
		LM_ERR("while sending request with t_request\n");
		if (uac_r.dialog != NULL)
		{
			uac_r.dialog->rem_target.s = 0;
			uac_r.dialog->dst_uri.s = 0;
			tmb.free_dlg(uac_r.dialog);
			uac_r.dialog = 0;
		}
		
		/* Although this is an error must not return -1 as the
		   calling function must continue processing. */
		ret = 0;
		goto error;
	}

	uac_r.dialog->rem_target.s = 0;
	uac_r.dialog->dst_uri.s = 0;
	tmb.free_dlg(uac_r.dialog);
	uac_r.dialog = 0;
	ret = 0;

error:
	pkg_free(str_hdr);

	return ret;	
}

str* subs_build_hdr(str* contact, int expires, str* extra_headers)
{
	str* str_hdr= NULL;
	static char buf[3000];
	char* subs_expires= NULL;
	int len= 1;

	str_hdr= (str*)pkg_malloc(sizeof(str));
	if(str_hdr== NULL)
	{
		LM_ERR("no more memory\n");
		return NULL;
	}
	memset(str_hdr, 0, sizeof(str));
	str_hdr->s= buf;
	
	memcpy(str_hdr->s ,"Max-Forwards: ", 14);
	str_hdr->len = 14;
	str_hdr->len+= sprintf(str_hdr->s+ str_hdr->len,"%d", MAX_FORWARD);
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	memcpy(str_hdr->s+ str_hdr->len ,"Event: dialog", 13);
	str_hdr->len+= 13;
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;
	
	memcpy(str_hdr->s+ str_hdr->len ,"Contact: <", 10);
	str_hdr->len += 10;
	memcpy(str_hdr->s +str_hdr->len, contact->s, 
			contact->len);
	str_hdr->len+= contact->len;
	memcpy(str_hdr->s+ str_hdr->len, ">", 1);
	str_hdr->len+= 1;
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	memcpy(str_hdr->s+ str_hdr->len ,"Expires: ", 9);
	str_hdr->len += 9;

	subs_expires= int2str(expires, &len);
		
	if(subs_expires == NULL || len == 0)
	{
		LM_ERR("while converting int to str\n");
		pkg_free(str_hdr);
		return NULL;
	}
	memcpy(str_hdr->s+str_hdr->len, subs_expires, len);
	str_hdr->len += len;
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	if(extra_headers && extra_headers->len)
	{
		memcpy(str_hdr->s+str_hdr->len, extra_headers->s, extra_headers->len);
		str_hdr->len += extra_headers->len;
	}

	str_hdr->s[str_hdr->len]= '\0';

	return str_hdr;
}

subs_info_t* subscribe_cbparam(subs_info_t* subs)
{
	subs_info_t* hentity= NULL;
	int size;

	size = sizeof(subs_info_t);
	if(subs->pres_uri && subs->pres_uri->len && subs->pres_uri->s )
		size+= sizeof(str)+ subs->pres_uri->len* sizeof(char);

	if(subs->watcher_uri && subs->watcher_uri->len && subs->watcher_uri->s )
		size+= sizeof(str)+ subs->watcher_uri->len* sizeof(char);

	if(subs->contact && subs->contact->len && subs->contact->s )
		size+= sizeof(str)+ subs->contact->len* sizeof(char);

	if(subs->outbound_proxy && subs->outbound_proxy->len && subs->outbound_proxy->s )
		size+= sizeof(str)+ subs->outbound_proxy->len* sizeof(char);

	if(subs->extra_headers && subs->extra_headers->s)
		size+= sizeof(str)+ subs->extra_headers->len* sizeof(char);

	hentity= (subs_info_t*)shm_malloc(size);
	if(hentity== NULL)
	{
		LM_ERR("No more share memory\n");
		return NULL;
	}
	memset(hentity, 0, size);

	size= sizeof(subs_info_t);

	hentity->pres_uri = (str*)((char*)hentity + size);
	size+= sizeof(str);

	hentity->pres_uri->s = (char*)hentity+ size;
	memcpy(hentity->pres_uri->s, subs->pres_uri->s ,
		subs->pres_uri->len ) ;
	hentity->pres_uri->len= subs->pres_uri->len;
	size+= subs->pres_uri->len;

	hentity->watcher_uri = (str*)((char*)hentity + size);
	size+= sizeof(str);

	hentity->watcher_uri->s = (char*)hentity+ size;
	memcpy(hentity->watcher_uri->s, subs->watcher_uri->s ,
		subs->watcher_uri->len ) ;
	hentity->watcher_uri->len= subs->watcher_uri->len;
	size+= subs->watcher_uri->len;

	hentity->contact = (str*)((char*)hentity + size);
	size+= sizeof(str);

	hentity->contact->s = (char*)hentity+ size;
	memcpy(hentity->contact->s, subs->contact->s ,
		subs->contact->len );
	hentity->contact->len= subs->contact->len;
	size+= subs->contact->len;

	if(subs->outbound_proxy)
	{
		hentity->outbound_proxy= (str*)((char*)hentity+ size);
		size+= sizeof(str);
		hentity->outbound_proxy->s= (char*)hentity+ size;
		memcpy(hentity->outbound_proxy->s, subs->outbound_proxy->s, subs->outbound_proxy->len);
		hentity->outbound_proxy->len= subs->outbound_proxy->len;
		size+= subs->outbound_proxy->len;
	}

	if(subs->extra_headers)
	{
		hentity->extra_headers= (str*)((char*)hentity+ size);
		size+= sizeof(str);
		hentity->extra_headers->s= (char*)hentity+ size;
		memcpy(hentity->extra_headers->s, subs->extra_headers->s,
				subs->extra_headers->len);
		hentity->extra_headers->len= subs->extra_headers->len;
		size+= subs->extra_headers->len;
	}

	hentity->expires= subs->expires;
	return hentity;	
}

void subs_cback_func(struct cell *t, int type, struct tmcb_params *ps)
{
	struct sip_msg* msg= NULL;
	subs_info_t* hentity= NULL;
	struct sip_uri uri;

	LM_INFO("Hybrid dialog subscribe completed with status %d\n", ps->code) ;

	if( ps->param== NULL || *ps->param== NULL )
	{
		LM_ERR("null callback parameter\n");
		return;
	}

	hentity= (subs_info_t*)(*ps->param);
	
	/* get dialog information from reply message: callid, to_tag, from_tag */
	msg= ps->rpl;
	if(msg == NULL)
	{
		LM_ERR("no reply message found\n ");
		goto end;
	}

	if(msg== FAKED_REPLY)
	{
		goto end;
	}
	
	if(parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LM_ERR("when parsing headers\n");
		goto end;
	}

	// If code responded as timeout. Considered this as dialog from this phone as terminated
	if(ps->code == 408)
	{
		if(parse_uri(hentity->pres_uri->s, hentity->pres_uri->len, &uri)< 0)
		{
			LM_ERR("while parsing uri\n");
			goto end;
		}

		if( collate_plugin.collate_handle_queue_dialog(collate_handle, &uri.user, &uri.host, NULL) < 0 ) {
      		LM_ERR("cannot queue dialog body to collator\n");
      		goto end;
    	}
	}

end:
	if(hentity)
	{	
		shm_free(hentity);
		hentity= NULL;
	}
	return;
}

int init_dialog_timers(void) 
{
	register_timer(dialog_active_check_handler, 0, dialog_check_period);

	if(enable_dialog_collate) {
		if ((dc_timer = timer_alloc()) == NULL) {
			LM_ERR("could not allocate dialog timer\n");
			return -1;
		}
	}

	return 0;
}

void clean_dialog_timers(void)
{
	if (dc_timer) {
		timer_free(dc_timer);
		dc_timer = NULL;
	}
}

void dialog_active_check_handler(unsigned int ticks,void *param)
{
	pres_uri_list_t* pres_uri_list;
	pres_uri_list_t* pres_uri_current;

	pres_uri_list = get_active_dialog();
	if(pres_uri_list== NULL)
	{
		LM_INFO("No active watchers found\n");
		return;
	}
	pres_uri_current = pres_uri_list;

	while (pres_uri_current) 
	{
		if(collate_plugin.collate_handle_is_active(collate_handle, &pres_uri_current->user, &pres_uri_current->domain) != -1) {
			LM_INFO("Sending subscribe to active dialog: %.*s\n"
				, pres_uri_current->pres_uri.len, pres_uri_current->pres_uri.s);
			if(send_subscribe0(pres_uri_current->user, pres_uri_current->domain) < 0) 
			{
				LM_ERR("Error sending subscribe to active dialog: %.*s\n"
					, pres_uri_current->pres_uri.len, pres_uri_current->pres_uri.s);
			}
		}
		pres_uri_current = pres_uri_current->next;
	}

	if(enable_dialog_collate) {
		timer_init(dc_timer, dialog_active_collate_handler, 0, F_TIMER_FAST);
		timer_add(dc_timer, S_TO_TICKS(dialog_collate_period));
	}

	free_pres_uri_list(pres_uri_list);
}

ticks_t dialog_active_collate_handler(ticks_t ticks, struct timer_ln* tl, void *p)
{
	// TODO
	// Preferrably you could pass this from the callback param
	// but somehow im getting a crashed when passing this list
	// need to review the kamailio callback
	pres_uri_list_t* pres_uri_list;
	pres_uri_list_t* pres_uri_current;

	pres_uri_list = get_active_dialog();
	if(pres_uri_list== NULL)
	{
		LM_INFO("No active watchers found\n");
		return 0;
	}
	pres_uri_current = pres_uri_list;

	while (pres_uri_current) 
	{
		str* tmp_body= NULL;
    	str* n_body= NULL;
    	publ_info_t publ;
    
    	memset(&publ, 0 , sizeof(publ_info_t));

        tmp_body = (str*)pkg_malloc(sizeof(str));
        if(tmp_body == NULL) {
        	LM_ERR("No more private memory\n");
        	goto end;
        }

        n_body = collate_plugin.collate_queue_xml(collate_handle, tmp_body, &pres_uri_current->user, &pres_uri_current->domain);
        if(n_body != NULL) {
        	LM_DBG("Collate Body is %.*s\n", n_body->len, n_body->s);
        	
        	publ.pres_uri = &pres_uri_current->pres_uri;
        	publ.body = n_body;

        	if (send_publish(&publ) < 0) {
        		LM_ERR("Send publish failed\n");
        	}

        	collate_plugin.free_collate_body(collate_handle, n_body->s);
        }
		
		pkg_free(tmp_body);
		pres_uri_current = pres_uri_current->next;
	}

end:
	free_pres_uri_list(pres_uri_list);
	return 0;
}

pres_uri_list_t* get_active_dialog()
{
	pres_uri_list_t* pres_uri_list = NULL;
	pres_uri_list_t* pres_uri_current = NULL;

	db_key_t result_cols[2];
	db1_res_t *result = NULL;
	db_row_t *row = NULL ;	
	db_val_t *row_vals = NULL;
	int n_result_cols = 0;
	int user_col, domain_col, presentity_col;
	int i;

	result_cols[user_col=n_result_cols++] = &str_to_user_col;
	result_cols[domain_col=n_result_cols++] = &str_to_domain_col;
	result_cols[presentity_col=n_result_cols++] = &str_presentity_uri_col;

	if (pa_dbf.use_table(pa_db, &active_watchers_table) < 0) 
	{
		LM_ERR("in use_table\n");
		goto error;
	}

	if (pa_dbf.query (pa_db, NULL, NULL, NULL,
		 result_cols, 0, n_result_cols, 0,  &result) < 0) 
	{
		LM_ERR("querying active_watchers db table\n");
		goto error;
	}

	if(result== NULL )
	{
		goto error;
	}

	if(result->n <= 0)
	{
		LM_DBG("The query in db table for active subscription"
				" returned no result\n");
		pa_dbf.free_result(pa_db, result);
		return 0;
	}

	for(i=0; i<result->n; i++)
	{
		str user;
		str domain;
		str pres_uri;
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);

		user.s= (char*)row_vals[user_col].val.string_val;
		user.len= strlen(user.s);

		domain.s= (char*)row_vals[domain_col].val.string_val;
		domain.len= strlen(domain.s);

		pres_uri.s= (char*)row_vals[presentity_col].val.string_val;
		pres_uri.len= strlen(pres_uri.s);

		if(pres_uri_list) {
			if(search_active_dialog(pres_uri_list, user, domain) == NULL) {
				if(add_active_dialog(user, domain, pres_uri, pres_uri_current) < 0) {
					LM_ERR("no more memory\n");
					goto error;
				}
			}	
		} else {
			pres_uri_list = new_active_dialog(user, domain, pres_uri);
			pres_uri_current = pres_uri_list;
		}
	}
	
	pa_dbf.free_result(pa_db, result);

	return pres_uri_list;

error:
	if(result)
		pa_dbf.free_result(pa_db, result);

	if(pres_uri_list)
		free_pres_uri_list(pres_uri_list);

	return NULL;
}

pres_uri_list_t* new_active_dialog(str pres_user, str pres_domain, str pres_uri)
{
	pres_uri_list_t* p;

	p= (pres_uri_list_t*)pkg_malloc(sizeof(pres_uri_list_t));
	if(p== NULL)
	{
		LM_ERR("No more private memory\n");
		return NULL;
	}
	memset(p, 0, sizeof(pres_uri_list_t));

	p->user.s = (char*)pkg_malloc(pres_user.len+ 1);
	if(p->user.s == NULL)
	{
		LM_ERR("no more memory\n");
		goto error;
	}
	memcpy(p->user.s, pres_user.s, pres_user.len);
	p->user.len = pres_user.len;
	p->user.s[p->user.len] = '\0';

	p->domain.s = (char*)pkg_malloc(pres_domain.len+ 1);
	if(p->domain.s == NULL)
	{
		LM_ERR("no more memory\n");
		goto error;
	}
	memcpy(p->domain.s, pres_domain.s, pres_domain.len);
	p->domain.len = pres_domain.len;
	p->domain.s[p->domain.len] = '\0';

	p->pres_uri.s = (char*)pkg_malloc(pres_uri.len+ 1);
	if(p->pres_uri.s == NULL)
	{
		LM_ERR("no more memory\n");
		goto error;
	}
	memcpy(p->pres_uri.s, pres_uri.s, pres_uri.len);
	p->pres_uri.len = pres_uri.len;
	p->pres_uri.s[p->pres_uri.len] = '\0';
	return p;

error:
	if(p)
	{
		if(p->user.s)
			pkg_free(p->user.s);

		if(p->domain.s)
			pkg_free(p->domain.s);

		if(p->pres_uri.s)
			pkg_free(p->pres_uri.s);

		pkg_free(p);
	}
	return NULL;	
}

int add_active_dialog(str pres_user, str pres_domain, str pres_uri, pres_uri_list_t* pres_list)
{
	pres_uri_list_t* p;

	p = new_active_dialog(pres_user, pres_domain, pres_uri);
	if(p== NULL)
	{
		LM_ERR("No more private memory\n");
		return -1;
	}
	p->next= pres_list->next;
	pres_list->next= p;
	return 0;
}

pres_uri_list_t* search_active_dialog(pres_uri_list_t* pres_list, str pres_user, str pres_domain)
{
	pres_uri_list_t* current = pres_list;
	while (current != NULL) {
		if( current->user.s && current->domain.s &&
			strcmp(current->user.s, pres_user.s) == 0 && 
			strcmp(current->domain.s, pres_domain.s) == 0) {
			return current;
		}
    	current = current->next;
	}
	return current;
}

void free_pres_uri_list(pres_uri_list_t * pres_list) 
{
	pres_uri_list_t* p;
	while(pres_list)
	{
		p= pres_list;
		if(p->user.s !=NULL)
			pkg_free(p->user.s);
		if(p->domain.s !=NULL)
			pkg_free(p->domain.s);
		if(p->pres_uri.s)
			pkg_free(p->pres_uri.s);
		pres_list= pres_list->next;
		pkg_free(p);
	}

	pres_list = NULL;
}