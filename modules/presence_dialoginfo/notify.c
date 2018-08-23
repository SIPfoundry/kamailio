/*
 * pua_reginfo module - Presence-User-Agent Handling of reg events
 *
 * Copyright (C) 2011-2012 Carsten Bock, carsten@ng-voice.com
 * http://www.ng-voice.com
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

#include "notify.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_uri.h"
#include "../../modules/usrloc/usrloc.h"
#include "../../lib/srutils/sruid.h"
#include "dialog_collator.h"
#include "send_publish.h"
#include "presence_dialoginfo.h"

int dialog_handle_notify(struct sip_msg* msg, char* p1, char* p2) {
 	str body;
  sip_uri_t *furi = NULL;
  to_body_t *tb = NULL;
  str* tmp_body= NULL;
  str* n_body= NULL;
  publ_info_t publ;
  int ret = 1;

	/* If not done yet, parse the whole message now: */
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("Error parsing headers\n");
		return -1;
	}

  /* check for To and From headesr */
  if((furi = parse_from_uri(msg)) == NULL)
  {
    LM_ERR("failed to find/parsed From headers\n");
    return -1;
  }

  tb = get_from(msg);

  LM_DBG("pres_uri=%.*s\n", tb->uri.len, tb->uri.s);

	if (get_content_length(msg) == 0) {
 		LM_DBG("Content length = 0\n");
    /* No Body? Then there is no dialog information available, which is ok. */
 		return ret;
 	} else {
 		body.s=get_body(msg);
 		if (body.s== NULL) {
 			LM_ERR("cannot extract body from msg\n");
 			return -1;
 		}
 		body.len = get_content_length(msg);

    LM_DBG("Body is %.*s\n", body.len, body.s);

    if(enable_dialog_collate) {
      if( collate_plugin.collate_handle_queue_dialog(collate_handle, &furi->user, &furi->host, &body) < 0 ) {
        LM_ERR("cannot queue dialog body to collator\n");
        return -1;
      }
    } else {
      memset(&publ, 0 , sizeof(publ_info_t));
      tmp_body = (str*)pkg_malloc(sizeof(str));
      if(tmp_body == NULL) {
        LM_ERR("No more private memory\n");
        return -1;
      }

      n_body = collate_plugin.collate_notify_xml(collate_handle, tmp_body, &furi->user, &furi->host, &body);
      if(n_body != NULL) {
        LM_DBG("Collate Notify is %.*s\n", n_body->len, n_body->s);
        
        publ.pres_uri = &tb->uri;
        publ.body = n_body;

        if (send_publish(&publ) < 0) {
          LM_ERR("Send publish failed\n");
          ret = -1;
        }
      }
    }
 	}

  if(n_body) {
    collate_plugin.free_collate_body(collate_handle, n_body->s);  
    pkg_free(n_body);    
  }

	return ret;
}

