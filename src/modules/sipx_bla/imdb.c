/*
 * sipx_bla module - imdb header file
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

#include "imdb.h"
#include "sipx_bla.h"

#include "../../lib/srdb1/db.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_from.h"
#include "../../core/ut.h"
#include "../../core/dset.h"
#include "../../core/route.h"
#include "../../core/pvar.h"
#include "../../core/str.h"

/*Extern global variables*/
db_func_t  im_dbf;
db1_con_t* im_db = NULL;

/*IMDB Column Names*/
static str entity_table_shared_column = str_init("shared");
static str entity_table_valid_user_column = str_init("vld");
static str entity_table_entity_column = str_init("ent");
static str entity_table_user_type = str_init("user");

void init_db_string(db_key_t* db_keys, db_val_t* db_values, int index, str* key, str* val);
void init_db_string_with_op(db_key_t* db_keys, db_val_t* db_values, db_op_t* db_op, int index, str* key, str* val, db_op_t op);

/* helper db functions*/
int im_db_bind(const str* db_url)
{
  if (db_bind_mod(db_url, &im_dbf )) 
  {
    LM_ERR("Cannot bind to database module!\n");
    return -1;
  }

  if (!DB_CAPABILITY(im_dbf, DB_CAP_ALL)) {
    LM_ERR("Database module does not implement all functions needed"
        " by the module\n");
    return -1;
  }

  return 0;
}

int im_db_init(const str* db_url)
{ 
  if (im_dbf.init==0){
    LM_ERR("Unbound database module\n");
    goto error;
  }

  if (im_db!=0)
    return 0;

  im_db=im_dbf.init(db_url);
  if (im_db==0){
    LM_ERR("Cannot initialize database connection\n");
    goto error;
  }

  return 0;

error:
  return -1;
}

int im_db_init2(const str* db_url)
{
  if (im_dbf.init==0){
    LM_ERR("Unbound database module\n");
    goto error;
  }

  if (im_db!=0)
    return 0;

  im_db = im_dbf.init2 ? im_dbf.init2(db_url, DB_POOLING_NONE) : im_dbf.init(db_url);
  if (im_db==0){
    LM_ERR("Cannot initialize database connection\n");
    goto error;
  }

  return 0;

error:
  return -1;  
}


void im_db_close(void)
{
  if (im_db && im_dbf.close){
    im_dbf.close(im_db);
    im_db=0;
  }
}

int is_user_supports_bla(str * user)
{
  db_key_t db_keys[2];
  db_val_t db_values[2];
  db_key_t db_filter[2];
  db1_res_t* db_res = NULL;
  db_row_t* row = NULL;
  db_val_t* value = NULL;
  int row_count = 0;
  int query_index = 0;
  int filter_index = 0;
  int index = 0;

  if(user == NULL)
  {
    LM_ERR("null user parameter\n");
    return(-1); 
  }

  /* init db table */
  if(im_db == NULL)
  {
    LM_ERR("null database connection\n");
    return(-1);
  }

  if (im_dbf.use_table(im_db, &db_table_entity) < 0)
  {
    LM_ERR("failed to use_table\n");
    return -1;
  }

  /*init query values*/
  init_db_string(db_keys, db_values, query_index++, &entity_table_entity_column, &entity_table_user_type);
  init_db_string(db_keys, db_values, query_index++, &entity_table_shared_column, user);

  /*init filter values*/
  db_filter[filter_index++] = &entity_table_shared_column;
  db_filter[filter_index++] = &entity_table_valid_user_column;

  if (im_dbf.query(im_db, db_keys, 0, db_values, db_filter, query_index, filter_index, NULL, &db_res) < 0) 
  {
    LM_ERR("error while querying database\n");
    return -1;
  }

  row_count = RES_ROW_N(db_res);
  if(row_count > 0)
  {
    row = RES_ROWS(db_res);

    for(index = 0; index < row_count; ++index) 
    {
      row = RES_ROWS(db_res) + index;
      if (ROW_N(row) != filter_index) 
      {
        LM_ERR("unexpected cell count for imdb.entity\n");
        goto error;
      }

      value = ROW_VALUES(row);

      if ((VAL_TYPE(value) != DB1_STRING) || (VAL_TYPE(value+1) != DB1_INT)) 
      {
        LM_ERR("unexpected cell types for imdb.entity\n");
          goto error;
      }

      if ((VAL_NULL(value) != 1) && VAL_INT(value+1) == 1) 
      {
        im_dbf.free_result(im_db, db_res);
        return 0;
      }       
    }
  }

error:
  im_dbf.free_result(im_db, db_res);
  return -1;
}

int get_all_bla_users(list_entry_t** users)
{
  db_key_t db_keys[2];
  db_val_t db_values[2];
  db_key_t db_filter[2];
  db_op_t db_operator[2];
  db1_res_t* db_res = NULL;
  db_row_t* row = NULL;
  db_val_t* value = NULL;
  list_entry_t *user_list = NULL;

  str empty_user = str_init("");
  str* tmp_str = NULL;

  const char * user = NULL;
  int user_len = 0;
  int row_count = 0;
  int query_index = 0;
  int filter_index = 0;
  int index = 0;

  if(users == NULL)
  {
    LM_ERR("null users parameter\n");
    return(-1); 
  }

  /* init db table */
  if(im_db == NULL)
  {
    LM_ERR("null database connection\n");
    return(-1);
  }

  if (im_dbf.use_table(im_db, &db_table_entity) < 0)
  {
    LM_ERR("failed to use_table\n");
    return -1;
  }

  /*init query values with operator*/
  init_db_string_with_op(db_keys, db_values, db_operator, query_index++
    , &entity_table_entity_column
    , &entity_table_user_type
    , OP_EQ);

  init_db_string_with_op(db_keys, db_values, db_operator, query_index++
    , &entity_table_shared_column
    , &empty_user
    , OP_NEQ);

  /*init filter values*/
    db_filter[filter_index++] = &entity_table_shared_column;
    db_filter[filter_index++] = &entity_table_valid_user_column;

  if (im_dbf.query(im_db, db_keys, db_operator, db_values, db_filter, query_index, filter_index, NULL, &db_res) < 0) 
  {
    LM_ERR("error while querying database\n");
    return -1;
  }

  row_count = RES_ROW_N(db_res);
  if(row_count > 0)
  {
    row = RES_ROWS(db_res);

    for(index = 0; index < row_count; ++index) 
    {
      row = RES_ROWS(db_res) + index;

      if (ROW_N(row) != filter_index) 
      {
        LM_ERR("unexpected cell count for imdb.entity\n");
        goto error;
      }

      value = ROW_VALUES(row);

      if ((VAL_TYPE(value) != DB1_STRING) || (VAL_TYPE(value+1) != DB1_INT)) 
      {
        LM_ERR("unexpected cell types for imdb.entity\n");
        goto error;
      }

      if ((VAL_NULL(value) != 1) && VAL_INT(value+1) == 1) 
      {
        user = VAL_STRING(value);
        user_len = strlen(user);
        if ((tmp_str = (str *)pkg_malloc(sizeof(str))) == NULL)
        {
          LM_ERR("out of private memory\n");
          goto error;
        }

        if ((tmp_str->s = (char *)pkg_malloc(sizeof(char) * user_len + 1)) == NULL)
        {
          pkg_free(tmp_str);
          LM_ERR("out of private memory\n");
          goto error;
        }
        
        memcpy(tmp_str->s, user, user_len);
        tmp_str->len = user_len;
        tmp_str->s[tmp_str->len] = '\0';

        user_list = list_insert(tmp_str, user_list, NULL);
      }       
    }
  }

  *users = user_list;
  im_dbf.free_result(im_db, db_res);
  return 0;

error:
  if(user_list)
    list_free(&user_list);
  
  im_dbf.free_result(im_db, db_res);
  return -1;
}

void init_db_string(db_key_t* db_keys, db_val_t* db_values, int index, str * key, str * val)
{
  if(db_keys && db_values && key)
  {
    db_keys[index] = key;
    VAL_TYPE(&db_values[index]) = DB1_STR;
    VAL_NULL(&db_values[index]) = 0;
    if(val) 
    {
      VAL_STR(&db_values[index]).s = val->s;
      VAL_STR(&db_values[index]).len = val->len;    
    }
  }
}

void init_db_string_with_op(db_key_t* db_keys, db_val_t* db_values, db_op_t* db_op, int index, str* key, str* val, db_op_t op)
{
  if(db_keys && db_values && key)
  {
    db_keys[index] = key;
    VAL_TYPE(&db_values[index]) = DB1_STR;
    VAL_NULL(&db_values[index]) = 0;

    if(val) {
      VAL_STR(&db_values[index]).s = val->s;
      VAL_STR(&db_values[index]).len = val->len;    
    }

    if(db_op) {
      db_op[index] = op ? op : OP_EQ;
    }
  } 
}