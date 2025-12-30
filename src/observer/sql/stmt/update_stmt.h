#pragma once

#include "common/sys/rc.h"
#include "sql/stmt/stmt.h"
#include "sql/stmt/filter_stmt.h"

class Table;
class Db;
class FilterStmt;
class FieldMeta;
/**
 * @brief 更新语句（支持单个字段更新和条件）
 * @ingroup Statement
 */
class UpdateStmt : public Stmt
{
public:
  // 默认构造函数
  UpdateStmt() = default;

  // 带参构造函数，用于初始化更新的表格、字段、值和条件
  UpdateStmt(Table *table, Value *values, int value_amount, FilterStmt *filter_stmt, FieldMeta *field_meta)
      : table_(table), values_(values), value_amount_(value_amount), filter_stmt_(filter_stmt), field_meta_(field_meta)
  {}
  ~UpdateStmt() override;

public:
  StmtType type() const override { return StmtType::UPDATE; }
  // 创建 UpdateStmt 对象的静态工厂函数
  static RC create(Db *db, UpdateSqlNode &update_sql, Stmt *&stmt);

   Table      *table() const { return table_; }
  Value      *values() const { return values_; }
  int         value_amount() const { return value_amount_; }
  FilterStmt *filter_stmt() const { return filter_stmt_; }
  FieldMeta  *field_meta() const { return field_meta_; }

private:
  Table      *table_        = nullptr;  // 目标表格名称
  Value      *values_       = nullptr;  // 更新的值
  int         value_amount_ = 0;
  FilterStmt *filter_stmt_  = nullptr;  // 筛选条件
  FieldMeta  *field_meta_   = nullptr;  // 更新域
};
