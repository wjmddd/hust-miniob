/**
 * @file update_stmt.cpp
 * @brief UPDATE语句的语义解析器（Resolver阶段）
 * 
 * 【update功能核心实现文件 - Resolver阶段】
 * 
 * 主要功能：
 * 1. 验证UPDATE语句中的表是否存在
 * 2. 验证要更新的字段是否存在
 * 3. 验证字段类型与更新值是否匹配（支持类型转换）
 * 4. 处理NULL值的特殊情况
 * 5. 解析WHERE子句条件
 * 
 * 处理流程：
 *   UPDATE语法树(UpdateSqlNode) -> 表存在性检查 -> 字段存在性检查 
 *                                      -> 类型检查/转换 -> 条件解析 -> UpdateStmt
 */

#include "sql/stmt/update_stmt.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "sql/stmt/filter_stmt.h"
#include "storage/db/db.h"
#include "storage/table/table.h"
#include "common/type/date_type.h"

/**
 * @brief UpdateStmt析构函数
 * @details 释放filter_stmt_资源
 */
UpdateStmt::~UpdateStmt()
{
  if (nullptr != filter_stmt_) {
    delete filter_stmt_;
    filter_stmt_ = nullptr;
  }
}

/**
 * @brief 创建UpdateStmt对象（UPDATE语句的语义解析核心方法）
 * 
 * 【update功能的核心入口 - Resolver阶段】
 * 
 * @param db      数据库实例指针
 * @param update  Parser阶段生成的UPDATE语法树节点
 * @param stmt    输出参数，创建成功后指向UpdateStmt对象
 * @return RC     成功返回SUCCESS，失败返回对应错误码
 * 
 * 校验点：
 * 1. RC::SCHEMA_TABLE_NOT_EXIST    - 表不存在
 * 2. RC::SCHEMA_FIELD_NOT_EXIST    - 字段不存在
 * 3. RC::NULL_CANT_INSERT          - 尝试向非空字段插入NULL
 * 4. RC::SCHEMA_FIELD_TYPE_MISMATCH - 类型不匹配且无法转换
 */
RC UpdateStmt::create(Db *db, UpdateSqlNode &update, Stmt *&stmt)
{
  // ======================== 第1步：基本参数验证 ========================
  // 获取表名和字段名
  const char *table_name = update.relation_name.c_str();
  const char *field_name = update.attribute_name.c_str();

  // 验证基本参数是否有效
  if (nullptr == db || nullptr == table_name || nullptr == field_name || 0 == update.value.length()) {
    LOG_WARN("Invalid argument. db=%p, table_name=%p, field_name=%p, value_length=%d",
        db, table_name, field_name, update.value.length());
    return RC::INVALID_ARGUMENT;
  }

  // ======================== 第2步：表存在性检查 ========================
  // 【update核心校验】检查要更新的表是否存在
  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    // 表不存在，返回错误
    LOG_WARN("No such table. db=%s, table_name=%s", db->name(), table_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  // ======================== 第3步：字段存在性检查 ========================
  // 【update核心校验】检查要更新的字段是否存在于表中
  const TableMeta &table_meta = table->table_meta();
  const FieldMeta *field      = table_meta.field(field_name);
  if (nullptr == field) {
    // 字段不存在，返回错误
    LOG_WARN("No such field. db=%s, field_name=%s", db->name(), field_name);
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }

  // ======================== 第4步：类型检查与转换 ========================
  // 【null功能相关】检查值类型是否匹配，处理NULL值和类型转换
  Value real_value;
  if (field->type() != update.value.attr_type()) {
    // 类型不匹配，尝试类型转换
    // NOTE: 下面这段代码是从 make_record 里拿的，改这个的时候记得连那里的一块改（不想封装了 qaq）
    
    // 【null功能核心】处理NULL值的情况
    if (update.value.is_null())  // 更新的值是NULL
    {
      // 检查该字段是否允许NULL值
      if (not field->nullable()) {  // 该列定义为 NOT NULL，不允许空值
        return RC::NULL_CANT_INSERT;
      }
      // 允许NULL，设置空值
      real_value = update.value;
      real_value.set_type(field->type());  // 设置空值对应的类型
      real_value.set_null_value();         // 设置NULL标记值
    } else {
      // 非NULL值，尝试类型转换
      // 例如：字符串"123"可以转换为整数123
      RC rc = Value::cast_to(update.value, field->type(), real_value);
      if (OB_FAIL(rc)) {
        // 类型转换失败
        LOG_WARN("failed to cast value. table name:%s,field name:%s,value:%s ",
          table_name, field->name(), update.value.to_string().c_str());
        return rc;
      }
    }
    // 使用转换后的值替换原值
    update.value = real_value;
  }

  // // check if the date is legal
  // if (update.value.attr_type() == AttrType::DATES && update.value.get_int() == 0) {
  //   return RC::VARIABLE_NOT_VALID;
  // }

  // ======================== 第5步：解析WHERE条件 ========================
  // 创建表名到表对象的映射，用于条件解析
  std::unordered_map<std::string, Table *> table_map;
  table_map.insert(std::pair<std::string, Table *>(std::string(table_name), table));

  // 解析WHERE子句，创建FilterStmt
  // FilterStmt::create 内部会验证条件中的字段是否存在
  FilterStmt *filter_stmt = nullptr;
  RC          rc          = FilterStmt::create(
      db, table, &table_map, update.conditions, filter_stmt);
  if (rc != RC::SUCCESS) {
    LOG_WARN("cannot construct filter stmt");
    return rc;
  }

  // ======================== 第6步：创建UpdateStmt对象 ========================
  // 所有校验通过，创建UpdateStmt对象
  // 包含：表对象、更新值、值数量、过滤条件、字段元数据
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
