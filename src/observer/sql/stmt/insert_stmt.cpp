/* Copyright (c) 2021OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2022/5/22.
//

#include "sql/stmt/insert_stmt.h"
#include "common/log/log.h"
#include "storage/db/db.h"
#include "storage/table/table.h"
#include "common/type/date_type.h"

class DateType;

InsertStmt::InsertStmt(Table *table, const std::vector<InsertTuple> tuples) : table_(table), tuples_(tuples) {}

RC InsertStmt::create(Db *db, const InsertSqlNode &inserts, Stmt *&stmt)
{
  const char *table_name = inserts.relation_name.c_str();
  if (nullptr == db || nullptr == table_name || inserts.tuples.empty()) {
    LOG_WARN("invalid argument. db=%p, table_name=%p, value_num=%d",
        db, table_name, static_cast<int>(inserts.tuples.size()));
    return RC::INVALID_ARGUMENT;
  }

  // check whether the table exists
  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }
  // 遍历每个需要插入的元组
  for (auto &values : inserts.tuples) {
    const int        value_num  = static_cast<int>(values.size());
    const TableMeta &table_meta = table->table_meta();
    const int        field_num  = table_meta.field_num() - table_meta.sys_field_num();

    // check field num
    if (field_num != value_num) {
      LOG_WARN("schema mismatch. value num=%d, field num in schema=%d", value_num, field_num);
      return RC::SCHEMA_FIELD_MISSING;
    }
    // 检查类型匹配
    const int sys_filed_num = table_meta.sys_field_num();
    for (int i = 0; i < field_num; i++) {
      const FieldMeta *field_meta = table_meta.field(i + sys_filed_num);
      const AttrType   field_type = field_meta->type();
      const AttrType   value_type = values[i].attr_type();
      if (value_type != field_type) {
        Value real_value;
        if (values[i].is_null())  // 插入的是一个空值
        {
          if (not field_meta->nullable()) {  // 所在列不允许插入空值
            return RC::NULL_CANT_INSERT;
          }
        } else {
          RC rc = Value::cast_to(values[i], field_meta->type(), real_value);
          if (OB_FAIL(rc)) {
            LOG_WARN("failed to cast value. table name:%s,field name:%s,value:%s ",
              table_meta.name(), field_meta->name(), values[i].to_string().c_str());
            return rc;
          }
        }
      }
    }
  }
  for (auto raws : inserts.tuples) {
    for (auto e : raws) {
      LOG_WARN("here %d",*e.data());
    }
  }
  // everything alright
  stmt = new InsertStmt(table, inserts.tuples);
  return RC::SUCCESS;
}
