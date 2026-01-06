/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
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

/**
 * @file filter_stmt.cpp
 * @brief WHERE子句过滤条件的语义解析器
 * 
 * 【select-meta功能相关 - WHERE条件字段校验】
 * 
 * 主要功能：
 * 1. 将WHERE子句中的条件解析为过滤表达式
 * 2. 验证条件中引用的表和字段是否存在
 * 3. 绑定字段到具体的表对象
 * 
 * 在select-meta中的作用：
 * - WHERE子句中引用不存在的字段时，在这里检测并返回错误
 * - 通过ExpressionBinder进行字段绑定，绑定失败说明字段不存在
 */

#include "sql/stmt/filter_stmt.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "common/sys/rc.h"
#include "storage/db/db.h"
#include "storage/table/table.h"
#include "sql/parser/expression_binder.h"

/** @brief FilterStmt析构函数，清空条件列表 */
FilterStmt::~FilterStmt() { conditions_.clear(); }

/**
 * @brief 创建FilterStmt对象（WHERE子句语义解析）
 * 
 * 【select-meta功能核心 - WHERE条件字段校验】
 * 
 * @param db            数据库实例
 * @param default_table 默认表（单表查询时使用）
 * @param tables        表名到表对象的映射
 * @param conditions    条件列表
 * @param stmt          输出的FilterStmt对象
 * @return RC           成功返回SUCCESS，失败返回错误码
 * 
 * 校验点：
 * - RC::SCHEMA_FIELD_NOT_EXIST - WHERE条件中的字段不存在
 * - RC::SCHEMA_TABLE_NOT_EXIST - WHERE条件中的表不存在
 */
RC FilterStmt::create(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
    std::vector<ConditionSqlNode> &conditions, FilterStmt *&stmt)
{
  // default_table 没有使用
  RC rc = RC::SUCCESS;
  stmt  = nullptr;

  // ======================== 第1步：将条件转换为表达式 ========================
  // 从 ConditionSqlNode 创建 ComparisonExpr
  // 支持的比较操作：=, <, >, <=, >=, <>, IS, IS NOT, IN, NOT IN, EXISTS, NOT EXISTS
  vector<unique_ptr<Expression>> conditions_exprs;
  for (auto &condition : conditions) {
    switch (condition.comp_op) {
      // 【null功能相关】IS和IS NOT用于NULL比较
      case CompOp::COMP_IS:
      case CompOp::COMP_IS_NOT:
      // 【子查询相关】EXISTS/NOT EXISTS/IN/NOT IN用于子查询
      case CompOp::EXISTS:
      case CompOp::NOT_EXISTS:
      case CompOp::IN:
      case CompOp::NOT_IN:
      // 常规比较操作
      case CompOp::EQUAL_TO:
      case CompOp::LESS_EQUAL:
      case CompOp::NOT_EQUAL:
      case CompOp::LESS_THAN:
      case CompOp::GREAT_EQUAL:
      case CompOp::GREAT_THAN: {
        // hint: 子查询会加入到这其中的一个 expr
        // 创建比较表达式，包含左右操作数和比较操作符
        conditions_exprs.emplace_back(std::make_unique<ComparisonExpr>(
            condition.comp_op, std::move(condition.left_expr), std::move(condition.right_expr)));

      } break;
      default: {
        // 不支持的比较操作符
        LOG_WARN("unsupported condition operator. comp_op=%d", condition.comp_op);
        return RC::INVALID_ARGUMENT;
      }
    }
  }

  // ======================== 第2步：创建绑定上下文 ========================
  // 将所有相关的表添加到绑定上下文中
  // 这样在绑定字段时可以在这些表中查找
  BinderContext binder_context;
  for (auto &table : *tables) {
    binder_context.add_table(table.second);
  }
  // 创建表达式绑定器
  ExpressionBinder expression_binder(binder_context);

  vector<unique_ptr<Expression>> bound_conditions;

  // ======================== 第3步：绑定表达式（字段校验） ========================
  // 【select-meta核心】在绑定过程中检查字段是否存在
  auto *tmp_stmt = new FilterStmt();
  for (size_t i = 0; i < conditions.size(); i++) {
    // 保存条件连接符（AND/OR），用于后续条件组合
    tmp_stmt->conjunction_types_.push_back(conditions[i].conjunction_type);
    
    // 【select-meta关键】绑定表达式
    // expression_binder.bind_expression 会：
    // 1. 解析字段引用（如 t1.id）
    // 2. 在表中查找字段
    // 3. 如果字段不存在，返回 SCHEMA_FIELD_NOT_EXIST 错误
    RC rc = expression_binder.bind_expression(conditions_exprs[i], bound_conditions);
    if (rc != RC::SUCCESS) {
      // 绑定失败（字段不存在等），释放资源并返回错误
      delete tmp_stmt;
      LOG_WARN("failed to create filter unit. condition index=%d", i);
      return rc;
    }
  }

  // 清理临时表达式
  conditions_exprs.clear();

  // ======================== 第4步：构建FilterStmt对象 ========================
  // 将绑定后的条件转移到FilterStmt中
  tmp_stmt->conditions_.swap(bound_conditions);

  stmt = tmp_stmt;
  return rc;
}

/**
 * @brief 获取表和字段对象（辅助函数）
 * 
 * 【select-meta功能核心 - 表和字段查找】
 * 
 * @param db            数据库实例
 * @param default_table 默认表（用于单表查询时省略表名的情况）
 * @param tables        表名到表对象的映射
 * @param attr          字段引用（包含表名和字段名）
 * @param table         输出：找到的表对象
 * @param field         输出：找到的字段元数据
 * @return RC           成功返回SUCCESS，失败返回错误码
 * 
 * 查找策略：
 * 1. 如果字段引用没有指定表名，使用默认表
 * 2. 如果指定了表名，在tables映射中查找
 * 3. 如果都找不到，在数据库中查找
 * 
 * 校验点：
 * - RC::SCHEMA_TABLE_NOT_EXIST - 表不存在
 * - RC::SCHEMA_FIELD_NOT_EXIST - 字段不存在
 */
RC get_table_and_field(Db *db, Table *default_table, unordered_map<string, Table *> *tables, const RelAttrSqlNode &attr,
    Table *&table, const FieldMeta *&field)
{
  // ======================== 第1步：查找表 ========================
  if (common::is_blank(attr.relation_name.c_str())) {
    // 没有指定表名，使用默认表（单表查询场景）
    // 例如：SELECT id FROM t; 中的 id 没有指定表名
    table = default_table;
  } else if (nullptr != tables) {
    // 指定了表名，在tables映射中查找
    // 例如：SELECT t1.id FROM t1; 中的 t1.id
    auto iter = tables->find(attr.relation_name);
    if (iter != tables->end()) {
      table = iter->second;
    }
  } else {
    // 备选方案：在数据库中查找表
    table = db->find_table(attr.relation_name.c_str());
  }
  
  // 【select-meta关键检查点】表不存在
  if (nullptr == table) {
    LOG_WARN("No such table: attr.relation_name: %s", attr.relation_name.c_str());
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  // ======================== 第2步：查找字段 ========================
  // 在找到的表中查找字段
  field = table->table_meta().field(attr.attribute_name.c_str());
  
  // 【select-meta关键检查点】字段不存在
  if (nullptr == field) {
    LOG_WARN("no such field in table: table %s, field %s", table->name(), attr.attribute_name.c_str());
    table = nullptr;  // 清空table，表示查找失败
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }

  return RC::SUCCESS;
}
