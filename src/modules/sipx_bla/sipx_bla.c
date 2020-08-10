/*
 * sipx_bla module - sipx_bla source file
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
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h> 
#include <signal.h>

#include "sipx_bla.h"
#include "imdb.h"
#include "cmd.h"
#include "notify.h"
#include "subscribe.h"
#include "message_queue.h"

#include "../../core/dprint.h"
#include "../../core/pt.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/timer.h"
#include "../../core/parser/msg_parser.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/sl/sl.h"
#include "../../lib/srutils/srjson.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_content.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/parse_uri.h" 
#include "../presence/event_list.h"
#include "../pua/pua.h"

#include "add_events.h"

MODULE_VERSION

#define MAX_FILEPATH_SIZE 512

/** module parameters */
str db_sipx_im_url = {NULL, 0};
str db_table_entity = str_init("entity");
str server_address = {NULL, 0};
str outbound_proxy = {NULL, 0};
str bla_header_name = {NULL, 0};

int poll_sipx_bla_user = 0; /*if 1, module will automatically register a timer based on poll_sipx_interval. 
                    This will fetch bla contacts using dialog reg event
               */
int poll_sipx_interval = 60; /*Default to 60 seconds*/

/* plugin module parameters */
int use_bla_message_queue = 0;
int bla_message_queue_log_level = 2;
str bla_message_queue_log_file = str_init("");
str bla_message_queue_channel = str_init("SIPX.BLA.REGISTER");
str bla_message_queue_redis_address = {NULL, 0};
str bla_message_queue_redis_password = str_init("");
int bla_message_queue_redis_port = 6379;
int bla_message_queue_redis_db = 0;

str bla_message_queue_plugin_name = str_init("libRedisMessageQueue.so");

/*
** Detect whether or not we are building for a 32- or 64-bit (LP/LLP)
** architecture. There is no reliable portable method at compile-time.
*/
#if defined (__alpha__) || defined (__ia64__) || defined (__x86_64__) \
                  || defined (_WIN64)   || defined (__LP64__) \
                        || defined (__LLP64__)
str bla_message_queue_plugin_path = str_init("/usr/lib64/kamailio/integration");
#else
str bla_message_queue_plugin_path = str_init("/usr/lib/kamailio/integration");
#endif

void* handle_bla_message_queue_plugin = NULL;
message_queue_plugin_t bla_message_queue_plugin;

pua_api_t pua; /*!< Structure containing pointers to PUA functions*/
presence_api_t pres; /*!< Structure containing pointers to PRESENCE functions*/
sl_api_t slb; /*!< Structure containing pointers to SL API functions*/

/** module functions */
static int mod_init(void);
static int child_init(int rank);
static void destroy(void);

/** child functions**/

/* Commands */
static cmd_export_t cmds[] = {
  {"is_sipx_bla_user", (cmd_function)sipx_is_bla_user_cmd, 1, bla_user_fixup, 0, REQUEST_ROUTE|ONREPLY_ROUTE},
  {"subscribe_sipx_bla_users", (cmd_function)sipx_subscribe_bla_users_cmd, 0, 0, 0, REQUEST_ROUTE|ONREPLY_ROUTE},
  {"sipx_handle_reginfo_notify", (cmd_function)sipx_handle_reginfo_notify_cmd, 0, 0, 0, REQUEST_ROUTE},
  {"sipx_handle_bla_notify", (cmd_function)sipx_handle_bla_notify_cmd, 0, 0, 0, REQUEST_ROUTE},
  {0, 0, 0, 0, 0, 0} 
};

/* Params */
static param_export_t params[]={
  {"db_sipx_im_url", PARAM_STR, &db_sipx_im_url},
  {"db_table_entity", PARAM_STR, &db_table_entity},
  {"server_address", PARAM_STR, &server_address},
  {"outbound_proxy", PARAM_STR, &outbound_proxy},
  {"bla_header_name", PARAM_STR, &bla_header_name},
  {"poll_sipx_bla_user", INT_PARAM, &poll_sipx_bla_user},
  {"poll_sipx_interval", INT_PARAM, &poll_sipx_interval},
  {"use_bla_message_queue", INT_PARAM, &use_bla_message_queue},
  {"bla_message_queue_plugin_name", PARAM_STR, &bla_message_queue_plugin_name},
  {"bla_message_queue_plugin_path", PARAM_STR, &bla_message_queue_plugin_path},
  {"bla_message_queue_log_file", PARAM_STR, &bla_message_queue_log_file},
  {"bla_message_queue_log_level", INT_PARAM, &bla_message_queue_log_level },
  {"bla_message_queue_channel", PARAM_STR, &bla_message_queue_channel},
  {"bla_message_queue_redis_address", PARAM_STR, &bla_message_queue_redis_address},
  {"bla_message_queue_redis_password", PARAM_STR, &bla_message_queue_redis_password},
  {"bla_message_queue_redis_port", INT_PARAM, &bla_message_queue_redis_port},
  {"bla_message_queue_redis_db", INT_PARAM, &bla_message_queue_redis_db},
  {0, 0, 0}
};

/* Timer Callback */
static void subscribe_sipx_bla_users_timer(unsigned int ticks,void *param);

/** plugin load function */
static int sipx_plugin_init(void);
static void bla_message_event(char* channel, int channel_len, char* data, int data_len, void* privdata);

static int sipx_child_process(void);

struct module_exports exports= {
  "sipx_bla",     /* module name */
  DEFAULT_DLFLAGS,  /* dlopen flags */
  cmds,         /* exported functions */
  params,         /* exported parameters */
  0,              /* RPC method exports */
  0,              /* exported pseudo-variables */
  0,              /* response handling function */
  mod_init,       /* module initialization function */
  child_init,      /* destroy function */
  destroy      /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
  bind_pua_t bind_pua;
  bind_presence_t bind_presence;

  /* Check bla header name */
  if(!bla_header_name.s || bla_header_name.len<=0) 
  {
    LM_ERR("bla_header_name parameter not set\n");
    return -1;
  }

  /* Bind to PUA: */
  bind_pua= (bind_pua_t)find_export("bind_pua", 1,0);
  if (!bind_pua) 
  {
    LM_ERR("Can't bind pua\n");
    return -1;
  } 

  if (bind_pua(&pua) < 0) 
  {
    LM_ERR("Can't bind pua\n");
    return -1;
  }

  /* Check for Publish/Subscribe methods */
  if(pua.send_subscribe == NULL) 
  {
    LM_ERR("Could not import send_subscribe\n");
    return -1;
  }

  if(poll_sipx_bla_user == 1 || use_bla_message_queue == 1) 
  {
    bind_presence= (bind_presence_t)find_export("bind_presence", 1,0);
    if (!bind_presence) 
    {
        LM_ERR("can't bind presence\n");
        return -1;
    }
    if (bind_presence(&pres) < 0) 
    {
        LM_ERR("can't bind pua\n");
        return -1;
    }

    if(pres.add_event == NULL) 
    {
      LM_ERR("Could not import add_event\n");
      return -1;
    }
    
    if(dlginfo_add_events() < 0) {
      LM_ERR("unable to register dialog;sla events\n");
      return -1;
    }
  }

  /* Bind to SL API: */
  if (sl_load_api(&slb)!=0) 
  {
    LM_ERR("cannot bind to SL API\n");
    return -1;
  }

  if(poll_sipx_bla_user == 1)
  {
    /* Check if server address is defined */
    if(!server_address.s || server_address.len<=0) 
    {
      LM_ERR("server_address parameter not set\n");
      return -1;
    }

    /* Check kamailio database address */
    if(!db_sipx_im_url.s || db_sipx_im_url.len<=0) 
    {
      LM_ERR("db_sipx_im_url parameter not set\n");
      return -1;
    }

    /* Check IMDB connection */
    if (im_db_bind(&db_sipx_im_url)) {
      LM_ERR("no database module found. Have you configure the \"db_sipx_im_url\" modparam properly?\n");
      return -1;
    }

    if (im_db_init(&db_sipx_im_url) < 0) {
      LM_ERR("unable to open database connection\n");
      return -1;
    }

    register_timer(subscribe_sipx_bla_users_timer, 0, poll_sipx_interval);
  }

  register_procs(1);
  cfg_register_child(1);

  /* Close DB Connection */
  im_db_close();
  return 0;
}

static int child_init(int rank)
{
  int pid;

  if(use_bla_message_queue == 1 && rank==PROC_MAIN)
  {
    pid=fork_process(PROC_SIPINIT, "BLA Message Event", 1);
    if (pid<0)
    {
      LM_ERR("Create child Err [%d] \n", rank);
      return -1;
    }
    if(pid==0)
    {
      LM_INFO("Create child Success [%d] \n", rank);
      if (cfg_child_init())
      {
        LM_INFO("cfg chil init failed [%d] \n", rank);
        return -1;  
      }
      
      LM_INFO("spawn new child [%d] \n", rank);
      if(sipx_plugin_init() < 0)
      {
        LM_ERR("failed to init sipx plugin\n");
        return -1;
      }
      sipx_child_process();
      return 0;
    }

    return 0;
  }

  if (rank==PROC_INIT || rank==PROC_TCP_MAIN || rank==PROC_MAIN)
  {
    return 0; /* do nothing for the main process */
  }

  
  if(poll_sipx_bla_user == 1)
  {
    /*Set db connection per each child*/
    if (im_db_init2(&db_sipx_im_url) < 0) 
    {
      LM_ERR("unable to open database connection\n");
      return -1;
    }

    if (im_dbf.use_table(im_db, &db_table_entity) < 0)
    {
      LM_ERR("child %d: Error in use_table pua\n", rank);
      return -1;
    }

    LM_DBG("child %d: Database connection opened successfully\n", rank);
  }

  return 0;
}

static void destroy(void)
{
  if(poll_sipx_bla_user == 1)
  {
    im_db_close();
  }
}

static void subscribe_sipx_bla_users_timer(unsigned int ticks,void *param)
{
  if( sipx_subscribe_bla_users() < 0) 
  {
    LM_ERR("failed subscribing reg event to bla users\n");
  }
}

static int sipx_child_process(void)
{
  LM_DBG("Started worker process");

  while(1) {
    sleep(3);
  }

  return 0;
}

static int sipx_plugin_init(void)
{
  bind_message_queue_plugin_t bind_message_queue_plugin;
  str config_keys[7];
  str config_values[7];
  char handlePath[MAX_FILEPATH_SIZE];
  char ut_buf_int2str[INT2STR_MAX_LEN];
  int param_index = 0;
  char* error;

  str log_path_key = str_init("log-path");
  str log_level_key = str_init("log-level");
  str redis_address_key = str_init("redis-address");
  str redis_port_key = str_init("redis-port");;
  str redis_channel_key = str_init("redis-channel");
  str redis_password_key = str_init("redis-password");
  str redis_db_key = str_init("redis-db");
  str log_level_str = str_init("");
  str redis_port_str = str_init("");
  str redis_db_str = str_init("");

  if(use_bla_message_queue != 0) 
  {
    if((!bla_message_queue_plugin_name.s || !bla_message_queue_plugin_name.len)
            || (!bla_message_queue_plugin_path.s || !bla_message_queue_plugin_path.len)) 
    {
        LM_ERR("use_dialog_event_collator requires plugin name and path to be set\n");
        return -1;
    }
    
    memset(handlePath, 0, MAX_FILEPATH_SIZE);
    strcpy(handlePath, bla_message_queue_plugin_path.s);
    strcat(handlePath, "/");
    strcat(handlePath, bla_message_queue_plugin_name.s);
    
    handle_bla_message_queue_plugin = dlopen(handlePath, RTLD_NOW | RTLD_GLOBAL);
    if(!handle_bla_message_queue_plugin) 
    {
        LM_ERR("unable to load bla_message_queue_plugin_name : %s\n", (char*)dlerror());
        return -1;
    }
    
    bind_message_queue_plugin = (bind_message_queue_plugin_t)dlsym(handle_bla_message_queue_plugin, "bind_message_queue_plugin");
    if (((error =(char*)dlerror())!=0) || !bind_message_queue_plugin) 
    {
        LM_ERR("unable to load bind_message_queue_plugin: %s\n", error);
        return -1;
    }

    memset(&bla_message_queue_plugin, 0, sizeof(message_queue_plugin_t));
    if(bind_message_queue_plugin(&bla_message_queue_plugin) < 0)
    {
        LM_ERR("can't bind message queue plugin\n");
        return -1;
    }
    
    if(!bla_message_queue_plugin.message_queue_subscribe_event) {
        LM_ERR("must define message_queue_subscribe_event\n");
        return -1;
    }

    if(bla_message_queue_plugin.message_queue_plugin_init)
    {
        memset(config_keys, 0, sizeof(config_keys));
        memset(config_values, 0, sizeof(config_values));

        log_level_str.s = int2strbuf(bla_message_queue_log_level, ut_buf_int2str, INT2STR_MAX_LEN, &log_level_str.len);
        redis_port_str.s = int2strbuf(bla_message_queue_redis_port, ut_buf_int2str + log_level_str.len, INT2STR_MAX_LEN, &redis_port_str.len);
        redis_db_str.s = int2strbuf(bla_message_queue_redis_db, ut_buf_int2str + redis_port_str.len, INT2STR_MAX_LEN, &redis_db_str.len);

        /** Set config */
        config_keys[param_index] = log_path_key;
        config_values[param_index] = bla_message_queue_log_file;
        param_index++;

        config_keys[param_index] = log_level_key;
        config_values[param_index] = log_level_str;
        param_index++;    

        config_keys[param_index] = redis_address_key;
        config_values[param_index] = bla_message_queue_redis_address;
        param_index++;    

        config_keys[param_index] = redis_port_key;
        config_values[param_index] = redis_port_str;
        param_index++;            

        config_keys[param_index] = redis_password_key;
        config_values[param_index] = bla_message_queue_redis_password;
        param_index++;            

        config_keys[param_index] = redis_db_key;
        config_values[param_index] = redis_db_str;
        param_index++;        

        config_keys[param_index] = redis_channel_key;
        config_values[param_index] = bla_message_queue_channel;
        param_index++;                    
        
        if(bla_message_queue_plugin.message_queue_plugin_init(config_keys, config_values, param_index) < 0)
        {
            LM_ERR("unable to initialize message queue plugin\n");
            return -1;
        }

        if(bla_message_queue_plugin.message_queue_subscribe_event(&bla_message_queue_channel, (void*)&pua, bla_message_event) < 0)
        {
            LM_ERR("unable to subscribe channel on message queue plugin\n");
            return -1;
        }
    } 
  }

  return 0;
}

static void bla_message_event(char* channel, int channel_len, char* data, int data_len, void* privdata)
{
  subs_info_t subs;
  struct sip_uri parsed_contact_uri;
  char id_buf[512];
  int id_buf_len = 0;
  pua_api_t * pua = 0;

  srjson_doc_t *tdoc = NULL;
  srjson_t *it = NULL;
  str aor = str_init("");
  str contact = str_init("");
  int duration = 0;

  if(privdata == NULL)
  {
    LM_ERR("invalid privdata\n");
    return; 
  }

  tdoc = srjson_NewDoc(NULL);
  if(tdoc == NULL)
  {
    LM_ERR("no more memory\n");
    return;
  }

  pua = (pua_api_t *)privdata;

  tdoc->root = srjson_Parse(tdoc, data);
  if(tdoc->root == NULL) 
  {
    LM_ERR("invalid json doc [[%.*s]]\n", data_len, data);
    srjson_DeleteDoc(tdoc);
    return;
  }

  for(it=tdoc->root->child; it; it = it->next)
  {
    if (it->string == NULL) 
    {
      continue;
    }

    if (strcmp(it->string, "aor")==0) 
    {
      aor.s = it->valuestring;
      aor.len = strlen(aor.s);
    } else if (strcmp(it->string, "contact")==0) 
    {
      contact.s = it->valuestring;
      contact.len = strlen(contact.s);
    } else if (strcmp(it->string, "duration")==0) 
    {
      duration = SRJSON_GET_INT(it);
    } else 
    {
      LM_ERR("unrecognized field in json object\n");
    }
  }

  LM_INFO("Receive bla message event aor=%.*s, contact=%.*s, duration=%d"
    , aor.len, aor.s
    , contact.len, contact.s
    , duration);    

  if (parse_uri(contact.s, contact.len, &parsed_contact_uri) < 0) 
  {
    LM_ERR("failed to parse contact (%.*s)\n", contact.len, contact.s);
    return;
  }

  if(parsed_contact_uri.user.len<=0 || parsed_contact_uri.user.s==NULL
      || parsed_contact_uri.host.len<=0 || parsed_contact_uri.host.s==NULL)
  {
    LM_ERR("bad contact URI!\n");
    return;
  }  

  /*
     Send Subscribe
  */
  memset(&subs, 0, sizeof(subs_info_t));
  id_buf_len = snprintf(id_buf, sizeof(id_buf), "SIPX_BLA_SUBSCRIBE.%.*s", contact.len, contact.s);

  subs.id.s = id_buf;
  subs.id.len = id_buf_len;

  LM_DBG("SIPX BLA Subscribe Id: %.*s\n", id_buf_len, id_buf);
  
  subs.remote_target= &contact;
  subs.pres_uri= &aor;
  subs.watcher_uri  = &aor;
  subs.source_flag= BLA_SUBSCRIBE;
  subs.event= BLA_EVENT;
  subs.contact= &server_address;
  subs.expires = duration;
  subs.flag |= INSERT_TYPE;

  if(outbound_proxy.s && outbound_proxy.len)
    subs.outbound_proxy= &outbound_proxy;

  LM_INFO("Sending sipx bla subscribe to %.*s\n", contact.len, contact.s);
  if(pua->send_subscribe(&subs)< 0)
  {
    LM_ERR("while sending subscribe\n");
  }

  srjson_DeleteDoc(tdoc);
}

