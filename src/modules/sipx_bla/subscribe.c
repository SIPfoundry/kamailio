/*
 * sipx_bla module - subscribe source file
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

#include "subscribe.h"
#include "sipx_bla.h"
#include "imdb.h"

#include "../../core/parser/msg_parser.h"
#include "../pua/send_subscribe.h"
#include "../pua/pua.h"

/**
 * Utility method for searching the list_entry_t structure
 */
int list_search(str string, list_entry_t * list);

int sipx_reginfo_subscribe(str user)
{
	return sipx_reginfo_subscribe_with_expires(user, 180);
}

int sipx_reginfo_subscribe_with_expires(str user, int expires)
{
	subs_info_t subs;
	char id_buf[512];
	int id_buf_len = 0;
	
	if (user.len <= 0)
	{
		LM_ERR("user is empty\n");
		return -1;
	}

	LM_DBG("Subscribing to %.*s\n", user.len, user.s);

	memset(&subs, 0, sizeof(subs_info_t));
	id_buf_len = snprintf(id_buf, sizeof(id_buf), "SIPX_REG_SUBSCRIBE.%.*s", user.len, user.s);
	subs.id.s = id_buf;
	subs.id.len = id_buf_len;

	subs.remote_target = &user;
	subs.pres_uri= &user;
	subs.watcher_uri= &server_address;
	subs.expires = expires;

	subs.source_flag= REGINFO_SUBSCRIBE;
	subs.event= REGINFO_EVENT;
	subs.contact= &server_address;
	
	if(outbound_proxy.s && outbound_proxy.len)
		subs.outbound_proxy= &outbound_proxy;

	LM_INFO("Sending sipx reg subsribe to %.*s", user.len, user.s);
	if(pua.send_subscribe(&subs)< 0) 
	{
		LM_ERR("while sending subscribe\n");
		return -1;
	}	

	return 1;
}

int sipx_subscribe_bla_users()
{
	list_entry_t * bla_user_list = NULL;
	list_entry_t * reg_subs_list = NULL;
	str* tmp_str = NULL;
	int subs_result = 0;

	/** Retrieve subscribe bla users **/
	if(get_all_bla_users(&bla_user_list) < 0) 
	{
		LM_ERR("cannot retrieve sipx bla users\n");
		return -1;
	}

	/** Register reginfo event for new sipx bla users **/
	while ((tmp_str = list_pop(&bla_user_list)) != NULL)
	{
		reg_subs_list = get_sipx_reginfo_subscribes(*tmp_str);
		if(reg_subs_list != NULL)
		{
			LM_INFO("User %.*s is already subscribe to reg event\n", tmp_str->len, tmp_str->s);
			list_free(&reg_subs_list);
		} else 
    	{
			LM_INFO("Register reg event for %.*s\n", tmp_str->len, tmp_str->s);
			subs_result = sipx_reginfo_subscribe(*tmp_str);
			if(subs_result < 0) 
			{
				LM_WARN("Subscribe to reg event %.*s failed \n", tmp_str->len, tmp_str->s);
			}
		}

		pkg_free(tmp_str->s);
		pkg_free(tmp_str);
	}

	if(bla_user_list)
		list_free(&bla_user_list);

	return 0;
}

list_entry_t * get_sipx_reginfo_subscribes(str user)
{
	str did_str;
	char id_buf[512];
	int id_buf_len = 0;

	id_buf_len = snprintf(id_buf, sizeof(id_buf), "SIPX_REG_SUBSCRIBE.%.*s", user.len, user.s);
	did_str.s = id_buf;
	did_str.len = id_buf_len;
	return pua.get_subs_list(&did_str);
}

int list_search(str strng, list_entry_t * list)
{
	int cmp = 0;
	list_entry_t *p = list;

	if (list != NULL)
	{
		if (strncmp(p->strng->s, strng.s, strng.len) == 0)
		{
			return 0;
		} else
		{
			list_entry_t *p = list;

			while (p->next != NULL && (cmp = strncmp(p->next->strng->s, strng.s, strng.len)) < 0)
      {
				p = p->next;
      }

			if (cmp == 0)
			{
				return 0;
			}
		}
	}

	return -1;
}