/*
 * sipx_bla module - notify source file
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

#include "notify.h"
#include "sipx_bla.h"
#include "imdb.h"

#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_content.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_subscription_state.h"
#include "../../core/parser/parse_expires.h"
#include "../../core/strutils.h"
#include "../../lib/srutils/sruid.h"

#include <libxml/parser.h>

#define STATE_INIT 1
#define STATE_ACTIVE 2
#define STATE_TERMINATED 0
#define STATE_UNKNOWN -1

#define EVENT_UNKNOWN -1
#define EVENT_REGISTERED 0
#define EVENT_UNREGISTERED 1
#define EVENT_TERMINATED 2
#define EVENT_CREATED 3
#define EVENT_REFRESHED 4
#define EVENT_EXPIRED 5

void process_contact(str aor, str callid, int cseq, int expires, int event, str contact_uri) 
{
  subs_info_t subs;
  struct sip_uri parsed_contact_uri;

  char id_buf[512];
  int id_buf_len = 0;

  if(event != EVENT_REGISTERED 
    && event != EVENT_CREATED
    && event != EVENT_REFRESHED)
  {
    /* Contact is expired, deleted, unregistered, whatever: We do not need to do anything. */
    LM_DBG("Contact is expired, deleted or unregistered (%.*s)\n", contact_uri.len, contact_uri.s);
    return;
  }

  if (parse_uri(contact_uri.s, contact_uri.len, &parsed_contact_uri) < 0) 
  {
    LM_ERR("failed to parse contact (%.*s)\n", contact_uri.len, contact_uri.s);
    return;
  }

  if(parsed_contact_uri.user.len<=0 || parsed_contact_uri.user.s==NULL
      || parsed_contact_uri.host.len<=0 || parsed_contact_uri.host.s==NULL)
  {
    LM_ERR("bad contact URI!\n");
    return;
  }

  /*LM_DBG("Check BLA via AOR: %.*s -> %.*s\n"
    , aor.len, aor.s
    , contact_uri.len, contact_uri.s);
  if(is_user_supports_bla(&aor) < 0) 
  {
    LM_DBG("User is not a bla user: %.*s\n", aor.len, aor.s);
    return;
  }*/

  /*
     Send Subscribe
  */
  memset(&subs, 0, sizeof(subs_info_t));
  id_buf_len = snprintf(id_buf, sizeof(id_buf), "SIPX_BLA_SUBSCRIBE.%.*s", contact_uri.len, contact_uri.s);

  subs.id.s = id_buf;
  subs.id.len = id_buf_len;

  LM_DBG("SIPX BLA Subscribe Id: %.*s\n", id_buf_len, id_buf);
  
  subs.remote_target= &contact_uri;
  subs.pres_uri= &aor;
  subs.watcher_uri  = &aor;
  subs.source_flag= BLA_SUBSCRIBE;
  subs.event= BLA_EVENT;
  subs.contact= &server_address;
  subs.expires = 180;

  if(outbound_proxy.s && outbound_proxy.len)
    subs.outbound_proxy= &outbound_proxy;

  LM_INFO("Sending sipx bla subsribe to %.*s\n", contact_uri.len, contact_uri.s);
  if(pua.send_subscribe(&subs)< 0)
  {
    LM_ERR("while sending subscribe\n");
  }
}

xmlNodePtr xmlGetNodeByName(xmlNodePtr parent, const char *name) {
  xmlNodePtr cur = parent;
  xmlNodePtr match = NULL;
  while (cur) 
  {
    if (xmlStrcasecmp(cur->name, (unsigned char*)name) == 0)
      return cur;

    match = xmlGetNodeByName(cur->children, name);
    if (match)
      return match;

    cur = cur->next;
  }
  return NULL;
}

char * xmlGetAttrContentByName(xmlNodePtr node, const char* name) {
  xmlAttrPtr attr = node->properties;
  while (attr) 
  {
    if (xmlStrcasecmp(attr->name, (unsigned char*)name) == 0)
      return (char*)xmlNodeGetContent(attr->children);

    attr = attr->next;
  }
  return NULL;
}

int reginfo_parse_state(char * s) {
  if (s == NULL) 
  {
    return STATE_UNKNOWN;
  }
  switch (strlen(s)) 
  {
    case 4:
      if (strncmp(s, "init", 4) ==  0) return STATE_INIT;
      break;
    case 6:
      if (strncmp(s, "active", 6) ==  0) return STATE_ACTIVE;
      break;
    case 10:
      if (strncmp(s, "terminated", 10) ==  0) return STATE_TERMINATED;
      break;
    default:
      LM_ERR("Unknown State %s\n", s);
      return STATE_UNKNOWN;
  }

  LM_ERR("Unknown State %s\n", s);
  return STATE_UNKNOWN;
}

int reginfo_parse_event(char * s) 
{
  if (s == NULL) 
  {
    return EVENT_UNKNOWN;
  }

  switch (strlen(s)) 
  {
    case 7:
      if (strncmp(s, "created", 7) ==  0) return EVENT_CREATED;
      if (strncmp(s, "expired", 7) ==  0) return EVENT_EXPIRED;
      break;
    case 9:
      if (strncmp(s, "refreshed", 9) ==  0) return EVENT_CREATED;
      break;
    case 10:
      if (strncmp(s, "registered", 10) ==  0) return EVENT_REGISTERED;
      if (strncmp(s, "terminated", 10) ==  0) return EVENT_TERMINATED;
      break;
    case 12:
      if (strncmp(s, "unregistered", 12) ==  0) return EVENT_UNREGISTERED;
      break;
    default:
      LM_ERR("Unknown Event %s\n", s);
      return EVENT_UNKNOWN;
  }

  LM_ERR("Unknown Event %s\n", s);
  return EVENT_UNKNOWN;
}

int process_body(str notify_body) 
{
  xmlDocPtr doc= NULL;
  xmlNodePtr doc_root = NULL, registrations = NULL, contacts = NULL, uris = NULL;
  str aor = {0, 0};
  str callid = {0, 0};
  str contact_uri = {0, 0};
  str received = {0,0};
  str path = {0,0};
  str user_agent = {0, 0};
  int state, event, expires;
  char * expires_char,  * cseq_char;
  int cseq = 0;
  struct sip_uri parsed_aor;

  doc = xmlParseMemory(notify_body.s, notify_body.len);
  if(doc== NULL)  
  {
    LM_ERR("Error while parsing the xml body message, Body is:\n%.*s\n",
      notify_body.len, notify_body.s);
    return -1;
  }
  
  doc_root = xmlGetNodeByName(doc->children, "reginfo");
  if(doc_root == NULL) 
  {
    LM_ERR("while extracting the reginfo node\n");
    goto error;
  }

  registrations = doc_root->children;
  while (registrations) 
  {
    /* Only process registration sub-items */
    if (xmlStrcasecmp(registrations->name, BAD_CAST "registration") != 0)
      goto next_registration;

    state = reginfo_parse_state(xmlGetAttrContentByName(registrations, "state"));
    if (state == STATE_UNKNOWN) 
    {
      LM_ERR("No state for this contact!\n");   
      goto next_registration;
    } else if (state == STATE_INIT) 
    {
      LM_DBG("Init state event receive!\n");
      goto next_registration;
    }

    aor.s = xmlGetAttrContentByName(registrations, "aor");
    if (aor.s == NULL) 
    {
      LM_ERR("No AOR for this contact!\n");   
      goto next_registration;
    }

    aor.len = strlen(aor.s);
    LM_DBG("AOR %.*s has state \"%d\"\n", aor.len, aor.s, state);

    /* Get username part of the AOR, search for @ in the AOR. */
    if (parse_uri(aor.s, aor.len, &parsed_aor) < 0) 
    {
      LM_ERR("failed to parse Address of Record (%.*s)\n",
        aor.len, aor.s);
      goto next_registration;
    }

    if (state != STATE_TERMINATED) 
    {
      /* Now lets process the Contact's from this Registration: */
      contacts = registrations->children;
      while (contacts) 
      {
        if (xmlStrcasecmp(contacts->name, BAD_CAST "contact") != 0)
          goto next_contact;

        callid.s = xmlGetAttrContentByName(contacts, "callid");
        if (callid.s == NULL) 
        {
          LM_ERR("No Call-ID for this contact!\n");   
          goto next_contact;
        }

        callid.len = strlen(callid.s);
        received.s = xmlGetAttrContentByName(contacts, "received");
        if (received.s == NULL) 
        {
          LM_DBG("No received for this contact!\n");
          received.len = 0;
        } else 
        {
          received.len = strlen(received.s);
        }

        path.s = xmlGetAttrContentByName(contacts, "path"); 
        if (path.s == NULL) 
        {
          LM_DBG("No path for this contact!\n");
          path.len = 0;
        } else 
        {
          path.len = strlen(path.s);
        }

        user_agent.s = xmlGetAttrContentByName(contacts, "user_agent");
        if (user_agent.s == NULL) 
        {
          LM_DBG("No user_agent for this contact!\n");
          user_agent.len = 0;
        } else 
        {
          user_agent.len = strlen(user_agent.s);
        }

        event = reginfo_parse_event(xmlGetAttrContentByName(contacts, "event"));
        if (event == EVENT_UNKNOWN) 
        {
          LM_ERR("No event for this contact!\n");   
          goto next_contact;
        }

        expires_char = xmlGetAttrContentByName(contacts, "expires");
        if (expires_char != NULL) 
        {
          expires = atoi(expires_char);
          if (expires < 0) 
          {
            LM_ERR("No valid expires for this contact!\n");   
            goto next_contact;
          }         
        } else
        {
          LM_DBG("No expires for this contact!\n");   
          expires = 3600;
        }

        LM_DBG("%.*s: Event \"%d\", expires %d\n", callid.len, callid.s, event, expires);

        cseq_char = xmlGetAttrContentByName(contacts, "cseq");
        if (cseq_char == NULL) 
        {
          LM_WARN("No cseq for this contact!\n");   
        } else 
        {
          cseq = atoi(cseq_char);
          if (cseq < 0) 
          {
            LM_WARN("No valid cseq for this contact!\n");   
          }
        }

        /* Now lets process the URI's from this Contact: */
        uris = contacts->children;
        while (uris) 
        {
          if (xmlStrcasecmp(uris->name, BAD_CAST "uri") != 0)
            goto next_uri;

          contact_uri.s = (char*)xmlNodeGetContent(uris); 
          if (contact_uri.s == NULL) 
          {
            LM_ERR("No URI for this contact!\n");   
            goto next_registration;
          }

          contact_uri.len = strlen(contact_uri.s);
          LM_DBG("Contact: %.*s\n",
            contact_uri.len, contact_uri.s);

          process_contact(aor, callid, cseq, expires, event, contact_uri);
next_uri:
          uris = uris->next;
        }
next_contact:
        contacts = contacts->next;
      }
    }
next_registration:
    registrations = registrations->next;
  }

  /* Free the XML-Document */
  if(doc) 
    xmlFreeDoc(doc);

  return 1;

error:
  /* Free the XML-Document */
  if(doc) 
    xmlFreeDoc(doc);

  return -1;
}

int sipx_handle_reginfo_notify_cmd(struct sip_msg* msg, char* s1, char* s2) {
  str body;
  int result = 1;

  /* Check if we enable user poll*/
  if(poll_sipx_bla_user != 1)
  {
    LM_ERR("SIPX BLA poll is disabled\n");
    return -1;
  }

  /* If not done yet, parse the whole message now: */
  if (parse_headers(msg, HDR_EOH_F, 0) == -1) 
  {
    LM_ERR("Error parsing headers\n");
    return -1;
  }

  if (get_content_length(msg) == 0) 
  {
      LM_DBG("Content length = 0\n");
      /* No Body? Then there is no published information available, which is ok. */
      return 1;
  } else 
  {
    body.s=get_body(msg);
    if (body.s== NULL) 
    {
      LM_ERR("cannot extract body from msg\n");
      return -1;
    }
    body.len = get_content_length(msg);
  }

  LM_DBG("Body is %.*s\n", body.len, body.s);
  
  result = process_body(body);
  return result;
}

int sipx_handle_bla_notify_cmd(struct sip_msg* msg, char* s1, char* s2)
{
  publ_info_t publ;
  struct to_body *pto = NULL, TO = {0}, *pfrom = NULL;
  str body;
  ua_pres_t dialog;
  unsigned int expires= 0;
  struct hdr_field* hdr;
  str subs_state;
  int found= 0;
  str extra_headers= {0, 0};
  static char buf[255];
  str contact;

  memset(&publ, 0, sizeof(publ_info_t));
  memset(&dialog, 0, sizeof(ua_pres_t));
  
  if ( parse_headers(msg,HDR_EOH_F, 0)==-1 )
  {
    LM_ERR("parsing headers\n");
    return -1;
  }

  if( msg->to==NULL || msg->to->body.s==NULL)
  {
    LM_ERR("cannot parse TO header\n");
    goto error;
  }
  /* examine the to header */
  if(msg->to->parsed != NULL)
  {
    pto = (struct to_body*)msg->to->parsed;
    LM_DBG("'To' header ALREADY PARSED: <%.*s>\n",
        pto->uri.len, pto->uri.s );
  } else
  {
    parse_to(msg->to->body.s,msg->to->body.s + msg->to->body.len + 1, &TO);
    if(TO.uri.len <= 0)
    {
      LM_DBG("'To' header NOT parsed\n");
      goto error;
    }
    pto = &TO;
  }
  
  if (pto->tag_value.s==NULL || pto->tag_value.len==0 )
  {
    LM_ERR("NULL to_tag value\n");
    goto error;
  }
  
  if( msg->callid==NULL || msg->callid->body.s==NULL)
  {
    LM_ERR("cannot parse callid header\n");
    goto error;
  }
    
  if (!msg->from || !msg->from->body.s)
  {
    LM_ERR("cannot find 'from' header!\n");
    goto error;
  }
  if (msg->from->parsed == NULL)
  {
    LM_DBG(" 'From' header not parsed\n");
    /* parsing from header */
    if ( parse_from_header( msg )<0 )
    {
      LM_DBG(" ERROR cannot parse From header\n");
      goto error;
    }
  }

  pfrom = (struct to_body*)msg->from->parsed;
  if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
  {
    LM_ERR("no from tag value present\n");
    goto error;
  }
 
  dialog.pres_uri = &pfrom->uri;
  dialog.watcher_uri= &pto->uri;
  dialog.from_tag= pto->tag_value;
  dialog.to_tag= pfrom->tag_value;
  dialog.call_id = msg->callid->body;
  dialog.event= BLA_EVENT;
  dialog.flag= BLA_SUBSCRIBE;
  if(pua.is_dialog(&dialog)< 0)
  {
    LM_ERR("Notify in a non existing dialog\n");
    goto error;
  }

  LM_DBG("found a matching dialog\n");
 
  /* parse Subscription-State and extract expires if existing */
  hdr = msg->headers;
  while (hdr!= NULL)
  {
    if(cmp_hdrname_strzn(&hdr->name, "Subscription-State",18)==0)
    {
      found = 1;
      break;
    }
    hdr = hdr->next;
  }

  if(found==0 )
  {
    LM_ERR("No Subscription-State header found\n");
    goto error;
  }

  subs_state= hdr->body;
  if(strncmp(subs_state.s, "terminated", 10)== 0) 
  {
    expires= 0; 
  } else
  {
    if(strncmp(subs_state.s, "active", 6)== 0 ||
        strncmp(subs_state.s, "pending", 7)==0 )
    {
      char* sep= NULL;
      str exp= {0, 0};
      sep= strchr(subs_state.s, ';');
      if(sep== NULL)
      {
        LM_ERR("No expires found in Notify\n");
        goto error;
      }
      if(strncmp(sep+1, "expires=", 8)!= 0)
      {
        LM_ERR("No expires found in Notify\n");
        goto error;
      }
      exp.s= sep+ 9;
      sep= exp.s;
      while((*sep)>='0' && (*sep)<='9')
      {
        sep++;
        exp.len++;
      }
      if( str2int(&exp, &expires)< 0)
      {
        LM_ERR("while parsing int\n");
        goto error;
      }
    }
  }
   
  if ( get_content_length(msg) == 0 )
  {
    LM_ERR("content length= 0\n");
    goto error;
  }
  else
  {
    body.s=get_body(msg);
    if (body.s== NULL)
    {
      LM_ERR("cannot extract body from msg\n");
      goto error;
    }
    body.len = get_content_length( msg );
  }
    
  if(msg->contact== NULL || msg->contact->body.s== NULL)
  {
    LM_ERR("no contact header found");
    goto error;
  }
  if( parse_contact(msg->contact) <0 )
  {
    LM_ERR(" cannot parse contact header\n");
    goto error;
  }

  if(msg->contact->parsed == NULL)
  {
    LM_ERR("cannot parse contact header\n");
    goto error;
  }
  contact = ((contact_body_t* )msg->contact->parsed)->contacts->uri;

  /* build extra_headers with Sender*/
  extra_headers.s= buf;
  memcpy(extra_headers.s, bla_header_name.s, bla_header_name.len);
  extra_headers.len= bla_header_name.len;
  memcpy(extra_headers.s+extra_headers.len,": ",2);
  extra_headers.len+= 2;
  memcpy(extra_headers.s+ extra_headers.len, contact.s, contact.len);
  extra_headers.len+= contact.len;
  memcpy(extra_headers.s+ extra_headers.len, CRLF, CRLF_LEN);
  extra_headers.len+= CRLF_LEN;

  publ.pres_uri= &pto->uri;
  publ.body= &body;
  publ.source_flag= BLA_PUBLISH;
  publ.expires= expires;
  publ.event= BLA_EVENT;
  publ.extra_headers= &extra_headers;
  if(pua.send_publish(&publ)< 0)
  {
    LM_ERR("while sending Publish\n");
    goto error;
  }
  
  free_to_params(&TO);
  return 1;
   
error:
  free_to_params(&TO);
  return -1;
}

