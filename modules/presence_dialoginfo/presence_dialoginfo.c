/*
 * presence_dialoginfo module - Presence Handling of dialog events
 *
 * Copyright (C) 2007 Juha Heinanen
 * Copyright (C) 2008 Klaus Darilion, IPCom
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
 * History:
 * --------
 *  2008-08-25  initial version (kd)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../parser/msg_parser.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../presence/bind_presence.h"
#include "add_events.h"
#include "dialog_collator.h"
#include "presence_dialoginfo.h"

MODULE_VERSION

#define MAX_FILEPATH_SIZE 512

/* module functions */
static int mod_init(void);
static void mod_destroy(void);
static int child_init(int rank);

/* module variables */
add_event_t pres_add_event;

/* module parameters */
int force_single_dialog = 0;
int force_dummy_dialog = 0;

/* custom module parameters */
int use_dialog_event_collator = 0;
int dialog_collator_log_level = 2;
str dialog_collator_log_file = str_init("");
str dialog_collator_plugin_name = str_init("libdialogEventCollator.so");

/*collate_plugin_t
** Detect whether or not we are building for a 32- or 64-bit (LP/LLP)
** architecture. There is no reliable portable method at compile-time.
*/
#if defined (__alpha__) || defined (__ia64__) || defined (__x86_64__) \
	                || defined (_WIN64)   || defined (__LP64__) \
                        || defined (__LLP64__)
str dialog_collator_plugin_path = str_init("/usr/lib64/kamailio/integration");
#else
str dialog_collator_plugin_path = str_init("/usr/lib/kamailio/integration");
#endif

void* handle_collate_plugin = NULL;
void* collate_handle = NULL;
collate_plugin_t collate_plugin;

/* module exported commands */
static cmd_export_t cmds[] =
{
    {0,	0, 0, 0, 0, 0}
};

/* module exported paramaters */
static param_export_t params[] = {
	{ "force_single_dialog", INT_PARAM, &force_single_dialog },
	{ "force_dummy_dialog", INT_PARAM, &force_dummy_dialog },
    { "use_dialog_event_collator", INT_PARAM, &use_dialog_event_collator },
    { "dialog_collator_plugin_name", PARAM_STR, &dialog_collator_plugin_name },
    { "dialog_collator_plugin_path", PARAM_STR, &dialog_collator_plugin_path },
    { "dialog_collator_log_file", PARAM_STR, &dialog_collator_log_file },
    { "dialog_collator_log_level", INT_PARAM, &dialog_collator_log_level },
	{0, 0, 0}
};

/* module exports */
struct module_exports exports= {
    "presence_dialoginfo",		/* module name */
    DEFAULT_DLFLAGS,			/* dlopen flags */
    cmds,						/* exported functions */
    params,						/* exported parameters */
    0,							/* exported statistics */
    0,							/* exported MI functions */
    0,							/* exported pseudo-variables */
    0,							/* extra processes */
    mod_init,					/* module initialization function */
    0,							/* response handling function */
    mod_destroy,                /* destroy function */
    child_init,					/* per-child init function */
};
	
/*
 * init module function
 */
static int mod_init(void)
{
	presence_api_t pres;
	bind_presence_t bind_presence;
    char* error;
    char handlePath[MAX_FILEPATH_SIZE];

    bind_collate_plugin_t bind_collate_plugin;
    str log_path_key = str_init("log-path");
    str log_level_key = str_init("log-level");
    str collate_keys[2];
    str collate_values[2];
    str log_level;
    int param_index = 0;

	bind_presence= (bind_presence_t)find_export("bind_presence", 1,0);
	if (!bind_presence) {
		LM_ERR("can't bind presence\n");
		return -1;
	}
	if (bind_presence(&pres) < 0) {
		LM_ERR("can't bind pua\n");
		return -1;
	}

	pres_add_event = pres.add_event;
	if (pres_add_event == NULL) {
		LM_ERR("could not import add_event\n");
		return -1;
	}
	if(dlginfo_add_events() < 0) {
		LM_ERR("failed to add dialog-info events\n");
		return -1;		
	}
        
    if(use_dialog_event_collator != 0) 
    {
        if((!dialog_collator_plugin_name.s || !dialog_collator_plugin_name.len)
                || (!dialog_collator_plugin_path.s || !dialog_collator_plugin_path.len)) 
        {
            LM_ERR("use_dialog_event_collator requires plugin name and path to be set\n");
            return -1;
        }
        
        memset(handlePath, 0, MAX_FILEPATH_SIZE);
        strcpy(handlePath, dialog_collator_plugin_path.s);
        strcat(handlePath, "/");
        strcat(handlePath, dialog_collator_plugin_name.s);
        
        handle_collate_plugin = dlopen(handlePath, RTLD_NOW | RTLD_GLOBAL);
        if(!handle_collate_plugin) 
        {
            LM_ERR("unable to load dialog_collator_plugin_name : %s\n", (char*)dlerror());
            return -1;
        }
        
        bind_collate_plugin = (bind_collate_plugin_t)dlsym(handle_collate_plugin, "bind_collate_plugin");
        if (((error =(char*)dlerror())!=0) || !bind_collate_plugin) 
        {
            LM_ERR("unable to load collate_plugin: %s\n", error);
            return -1;
        }

        memset(&collate_plugin, 0, sizeof(collate_plugin_t));
        if(bind_collate_plugin(&collate_plugin) < 0)
        {
            LM_ERR("can't bind collate plugin\n");
            return -1;
        }
        
        if(!collate_plugin.collate_handle_init) 
        {
            LM_ERR("must define collate_init\n");
            return -1;
        }

        if(!collate_plugin.collate_body_xml) {
            LM_ERR("must define collate_body_xml\n");
            return -1;
        }

        if(collate_plugin.collate_plugin_init)
        {
            memset(collate_keys, 0, sizeof(collate_keys));
            memset(collate_values, 0, sizeof(collate_values));

            log_level.s = int2str(dialog_collator_log_level, &log_level.len);

            collate_keys[param_index] = log_path_key;
            collate_values[param_index] = dialog_collator_log_file;
            param_index++;

            collate_keys[param_index] = log_level_key;
            collate_values[param_index] = log_level;
            param_index++;    
            
            if(collate_plugin.collate_plugin_init(collate_keys, collate_values, param_index) < 0)
            {
                LM_ERR("unable to initialize collate plugin\n");
                return -1;
            }

            if(use_dialog_event_collator != 0) 
            {
                if (collate_plugin.collate_handle_init(&collate_handle) < 0) {
                    LM_ERR("unable to initialize collate handle\n");
                    return -1;
                }
            }
        }
        
    }
    
    return 0;
}

static int child_init(int rank)
{
    if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
        return 0; /* do nothing for the main process */

    return 0;
}

static void mod_destroy(void)
{
    //Cleanup collate plugin
    if(handle_collate_plugin != NULL)  
    {
        if(collate_plugin.collate_handle_destroy)
        {
            collate_plugin.collate_handle_destroy(collate_handle);
        }

        if(collate_plugin.collate_plugin_destroy)
        {
            collate_plugin.collate_plugin_destroy();
        }
        
        dlclose(handle_collate_plugin);
    }
}