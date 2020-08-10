/*
 * sipx_bla module - cmd source file
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

#include "cmd.h"
#include "imdb.h"
#include "subscribe.h"
#include "../../core/pvar.h"
#include "../../core/mod_fix.h"
#include "../rls/list.h"

int sipx_is_bla_user_cmd(struct sip_msg* msg, char* uri, char* param2)
{
  str uri_str = {0, 0};
  char uri_buf[512];
  int uri_buf_len = 512;
  
  if (pv_printf(msg, (pv_elem_t*)uri, uri_buf, &uri_buf_len) < 0) {
    LM_ERR("cannot print uri into the format\n");
    return -1;
  }

  uri_str.s = uri_buf;
  uri_str.len = uri_buf_len;
  return is_user_supports_bla(&uri_str);
}

int sipx_subscribe_bla_users_cmd(struct sip_msg* msg, char* param1, char* param2)
{
  return sipx_subscribe_bla_users();
}

int bla_user_fixup(void** param, int param_no)
{
  pv_elem_t *model;
  str s;
  if (param_no == 1) 
  {
    if(*param) 
    {
      s.s = (char*)(*param);
      s.len = strlen(s.s);
      if(pv_parse_format(&s, &model)<0) 
      {
        LM_ERR("wrong format[%s]\n",(char*)(*param));
        return E_UNSPEC;
      }
      *param = (void*)model;
      return 1;
    }

    LM_ERR("null format\n");
    return E_UNSPEC;
  } else {
    return 1;
  }
}
