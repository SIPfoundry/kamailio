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

#include "sipx_bla.h"
#include "imdb.h"
#include "cmd.h"
#include "notify.h"
#include "subscribe.h"

#include "../../pt.h"
#include "../../cfg/cfg_struct.h"
#include "../../timer.h"
#include "../../parser/msg_parser.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/sl/sl.h"
#include "../presence/event_list.h"

#include "add_events.h"

MODULE_VERSION

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

pua_api_t pua; /*!< Structure containing pointers to PUA functions*/
presence_api_t pres; /*!< Structure containing pointers to PRESENCE functions*/
sl_api_t slb; /*!< Structure containing pointers to SL API functions*/

/** module functions */
static int mod_init(void);
static int child_init(int rank);
static void destroy(void);

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
  {0, 0, 0}
};

/* Timer Callback */
static void subscribe_sipx_bla_users_timer(unsigned int ticks,void *param);

struct module_exports exports= {
  "sipx_bla",     /* module name */
  DEFAULT_DLFLAGS,  /* dlopen flags */
  cmds,         /* exported functions */
  params,         /* exported parameters */
  0,              /* exported statistics */
  0,              /* exported MI functions */
  0,              /* exported pseudo-variables */
  0,              /* extra processes */
  mod_init,       /* module initialization function */
  0,              /* response handling function */
  destroy,      /* destroy function */
  child_init      /* per-child init function */
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

    /* Check IMDB connection */
    if (im_db_bind(&db_sipx_im_url)) {
      LM_ERR("no database module found. Have you configure the \"db_sipx_im_url\" modparam properly?\n");
      return -1;
    }

    if (im_db_init(&db_sipx_im_url) < 0) {
      LM_ERR("unable to open database connection\n");
      return -1;
    }

    if(dlginfo_add_events() < 0) {
      LM_ERR("unable to register dialog;sla events\n");
      return -1;
    }

    register_timer(subscribe_sipx_bla_users_timer, 0, poll_sipx_interval);
  }

  /* Close DB Connection */
  im_db_close();
  return 0;
}

static int child_init(int rank)
{
  if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
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
