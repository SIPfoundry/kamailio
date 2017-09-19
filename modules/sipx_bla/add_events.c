/*
 * sipx_bla module - add_event source file
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include "../../parser/parse_content.h"
#include "../presence/event_list.h"
#include "sipx_bla.h"
#include "pidf.h"
#include "add_events.h"

#define MAX_INT_LEN 11 /* 2^32: 10 chars + 1 char sign */

static str pu_415_rpl  = str_init("Unsupported media type");

static str *dlginfo_body_setversion(subs_t *subs, str *body);
static void dlginfo_free_body(char* body);
static int xml_publ_handl(struct sip_msg* msg);

int dlginfo_add_events(void)
{
  pres_ev_t event;
  
  /* constructing message-summary event */
  memset(&event, 0, sizeof(pres_ev_t));
  event.name.s = "dialog;sla";
  event.name.len = 10;

  event.content_type.s= "application/dialog-info+xml";
  event.content_type.len= 27;

  event.etag_not_new = 1;
  event.default_expires= 3600;
  event.type = PUBL_TYPE;
  event.evs_publ_handl = xml_publ_handl;

  /* modify XML body for each watcher to set the correct "version" */
  event.aux_body_processing = dlginfo_body_setversion;
  event.aux_free_body = dlginfo_free_body;
  if (pres.add_event(&event) < 0) 
  {
    LM_ERR("failed to add event \"dialog\"\n");
    return -1;
  }   
  
  return 0;
}

static str *dlginfo_body_setversion(subs_t *subs, str *body) 
{
  xmlNodePtr node= NULL;
  xmlDocPtr doc= NULL;
  str* final_body=NULL;
  char* version;
  int len;

  if(!body)
  {
      return NULL;
  }

  doc= xmlParseMemory(body->s, body->len);
  if(doc== NULL)
  {
    LM_ERR("while parsing xml memory\n");
    goto error;
  }

  /* change version and state*/
  node= xmlDocGetNodeByName(doc, "dialog-info", NULL);
  if(node == NULL)
  {
    LM_ERR("while extracting dialog-info node\n");
    goto error;
  }

  version= int2str(subs->version,&len);
  version[len]= '\0';

  if(xmlSetProp(node, (const xmlChar *)"version",(const xmlChar*)version)== NULL)
  {
    LM_ERR("while setting version attribute\n");
    goto error; 
  }

  final_body= (str*)pkg_malloc(sizeof(str));
  if(final_body== NULL)
  {
    LM_ERR("NO more memory left\n");
    goto error;
  }
  memset(final_body, 0, sizeof(str));
  xmlDocDumpFormatMemory(doc, (xmlChar**)(void*)&final_body->s, &final_body->len, 1); 

  xmlFreeDoc(doc);
  xmlMemoryDump();
  xmlCleanupParser();
  return final_body;

error:
  if(doc)
      xmlFreeDoc(doc);
  if(body)
      pkg_free(body);
  
  xmlMemoryDump();
  xmlCleanupParser();
  return NULL;
}

static void dlginfo_free_body(char* body)
{
  if(body== NULL)
    return;

  xmlFree(body);
  body= NULL;
}

static int xml_publ_handl(struct sip_msg* msg)
{   
  str body= {0, 0};
  xmlDocPtr doc= NULL;

  if ( get_content_length(msg) == 0 )
      return 1;
  
  body.s=get_body(msg);
  if (body.s== NULL) 
  {
      LM_ERR("cannot extract body from msg\n");
      goto error;
  }
  /* content-length (if present) must be already parsed */

  body.len = get_content_length( msg );
  doc= xmlParseMemory( body.s, body.len );
  if(doc== NULL)
  {
      LM_ERR("bad body format\n");
      if(slb.freply(msg, 415, &pu_415_rpl) < 0)
      {
          LM_ERR("while sending '415 Unsupported media type' reply\n");
      }
      goto error;
  }
  xmlFreeDoc(doc);
  xmlCleanupParser();
  xmlMemoryDump();
  return 1;

error:
  xmlFreeDoc(doc);
  xmlCleanupParser();
  xmlMemoryDump();
  return -1;

}   
