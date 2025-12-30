#include "sql/stmt/update_stmt.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "sql/stmt/filter_stmt.h"
#include "storage/db/db.h"
#include "storage/table/table.h"
#include "common/type/date_type.h"

UpdateStmt::~UpdateStmt()
{
  if (nullptr != filter_stmt_) {
    delete filter_stmt_;
    filter_stmt_ = nullptr;
  }
}

RC UpdateStmt::create(Db *db, UpdateSqlNode &update, Stmt *&stmt)
{
  // Validate the arguments
  const char *table_name = update.relation_name.c_str();
  const char *field_name = update.attribute_name.c_str();

  if (nullptr == db || nullptr == table_name || nullptr == field_name || 0 == update.value.length()) {
    LOG_WARN("Invalid argument. db=%p, table_name=%p, field_name=%p, value_length=%d",
        db, table_name, field_name, update.value.length());
    return RC::INVALID_ARGUMENT;
  }

  // Check if the table exists
  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    LOG_WARN("No such table. db=%s, table_name=%s", db->name(), table_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  // Check if the field exists
  const TableMeta &table_meta = table->table_meta();
  const FieldMeta *field      = table_meta.field(field_name);
  if (nullptr == field) {
    LOG_WARN("No such field. db=%s, field_name=%s", db->name(), field_name);
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }
  // check if the type equals
  Value real_value;
  if (field->type() != update.value.attr_type()) {
    // LOG_WARN("field type mismatch, table=%s, field=%s, field type=%d, value_type=%d",
    // table_name,field->name(),field->type(),update.value.attr_type());
    // return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    // NOTE: 下面这段代码是从 make_record 里拿的，改这个的时候记得连那里的一块改（不想封装了 qaq）
    if (update.value.is_null())  // 插入的是一个空值
    {
      if (not field->nullable()) {  // 所在列不允许插入空值
        return RC::NULL_CANT_INSERT;
      }
      real_value = update.value;
      real_value.set_type(field->type());  // 设置空值对应的类型
      real_value.set_null_value();
    } else {
      RC rc = Value::cast_to(update.value, field->type(), real_value);
      if (OB_FAIL(rc)) {
        LOG_WARN("failed to cast value. table name:%s,field name:%s,value:%s ",
          table_name, field->name(), update.value.to_string().c_str());
        return rc;
      }
    }
    update.value = real_value;
  }

  // // check if the date is legal
  // if (update.value.attr_type() == AttrType::DATES && update.value.get_int() == 0) {
  //   return RC::VARIABLE_NOT_VALID;
  // }
  // handle the condition
  std::unordered_map<std::string, Table *> table_map;
  table_map.insert(std::pair<std::string, Table *>(std::string(table_name), table));

  FilterStmt *filter_stmt = nullptr;
  RC          rc          = FilterStmt::create(
      db, table, &table_map, update.conditions, filter_stmt);
  if (rc != RC::SUCCESS) {
    LOG_WARN("cannot construct filter stmt");
    return rc;
  }

  // Create the update statement with the relevant table, field, value, and conditions
  stmt = new UpdateStmt(table, (Value *)&update.value, 1, filter_stmt, (FieldMeta *)field);
  return RC::SUCCESS;
}

// void UpdateStmt::set_field_value(const char * field_name, const Value &value)
// {
//   field_name_ = field_name;
//   value_ = value;
// }

// void UpdateStmt::set_condition(std::vector<ConditionSqlNode> *condition)
// {
//   condition_ = condition;
// }
