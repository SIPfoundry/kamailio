/*
 * presence_dialoginfo module - dialog_collator header file
 *
 * Copyright (C) 2015 Ryan Colobong, SIPFoundry
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
 *  2015-10-16  initial version (ryan)
 */

#ifndef _DIALOG_COLLATOR_H_
#define _DIALOG_COLLATOR_H_

typedef int (*collate_plugin_init_t)(str* key, str* value, int size);
typedef int (*collate_plugin_destroy_t)();
typedef int (*collate_handle_init_t)(void** handle);
typedef int (*collate_handle_destroy_t)(void** handle);
typedef int (*collate_handle_is_active_t)(void* handle, str* pres_user, str* pres_domain);
typedef int (*collate_handle_queue_dialog_t)(void* handle, str* pres_user, str* pres_domain, str* body);
typedef str* (*collate_queue_xml_t)(void* handle, str* response, str* pres_user, str* pres_domain);
typedef str* (*collate_notify_xml_t)(void* handle, str* response, str* pres_user, str* pres_domain, str* body);
typedef str* (*collate_body_xml_t)(void* handle, str* response, str* pres_user, str* pres_domain, str** body_array, int n);
typedef void (*free_collate_body_t)(void* handle, char* body);

typedef struct collate_plugin_exports_ {
	collate_plugin_init_t collate_plugin_init;
	collate_plugin_destroy_t collate_plugin_destroy;
	collate_handle_init_t collate_handle_init;
	collate_handle_destroy_t collate_handle_destroy;
	collate_handle_is_active_t collate_handle_is_active;
	collate_handle_queue_dialog_t collate_handle_queue_dialog;
	collate_queue_xml_t collate_queue_xml;
	collate_notify_xml_t collate_notify_xml;
	collate_body_xml_t collate_body_xml;
	free_collate_body_t free_collate_body;
} collate_plugin_t;

int bind_collate_plugin(collate_plugin_t* api);
typedef int (*bind_collate_plugin_t)(collate_plugin_t* api);

#endif // ! _DIALOG_COLLATOR_H_