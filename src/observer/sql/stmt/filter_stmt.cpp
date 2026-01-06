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
 * @brief WHERE子句过滤条件的语义解析器（FilterStmt）
 * 
 * 【select-meta功能相关 - WHERE条件字段校验】
 * 【update功能相关 - WHERE条件过滤】
 * 
 * ============================================================
 * 什么是 FilterStmt？
 * ============================================================
 * 
 * FilterStmt 是 WHERE 子句的语义表示，用于在执行阶段过滤需要处理的记录。
 * 
 * 例如：UPDATE students SET age = 25 WHERE id = 1 AND score > 80;
 *                                       └────────────────────────┘
 *                                              这部分由 FilterStmt 表示
 * 
 * ============================================================
 * FilterStmt 的内部结构
 * ============================================================
 * 
 * FilterStmt
 * ├── conditions_              // 条件表达式列表
 * │   ├── [0] ComparisonExpr   // 第一个条件：id = 1
 * │   │       ├── left:  FieldExpr(id)
 * │   │       ├── op:    EQUAL_TO
 * │   │       └── right: ValueExpr(1)
 * │   │
 * │   └── [1] ComparisonExpr   // 第二个条件：score > 80
 * │           ├── left:  FieldExpr(score)
 * │           ├── op:    GREAT_THAN
 * │           └── right: ValueExpr(80)
 * │
 * └── conjunction_types_       // 条件连接符列表
 *     └── [0] AND              // 用 AND 连接两个条件
 * 
 * ============================================================
 * 在 UPDATE/SELECT/DELETE 中的作用
 * ============================================================
 * 
 * 1. Resolver阶段：创建 FilterStmt，校验字段是否存在
 * 2. Optimizer阶段：FilterStmt 被转换为 PredicateLogicalOperator
 * 3. Executor阶段：PredicatePhysicalOperator 使用条件过滤记录
 * 
 * 执行计划结构示例（UPDATE）：
 *   UpdatePhysicalOperator
 *       └── PredicatePhysicalOperator  ← 使用 FilterStmt 的条件过滤
 *               └── TableScanPhysicalOperator
 * 
 * ============================================================
 * 主要功能
 * ============================================================
 * 
 * 1. 将WHERE子句中的条件解析为过滤表达式（ComparisonExpr）
 * 2. 验证条件中引用的表和字段是否存在（select-meta校验）
 * 3. 绑定字段到具体的表对象
 * 4. 保存条件连接符（AND/OR）
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
 * 【update功能核心 - 创建过滤条件】
 * 
 * @param db            数据库实例
 * @param default_table 默认表（单表查询时使用）
 * @param tables        表名到表对象的映射
 * @param conditions    Parser阶段输出的条件列表（ConditionSqlNode）
 * @param stmt          输出的FilterStmt对象
 * @return RC           成功返回SUCCESS，失败返回错误码
 * 
 * 校验点：
 * - RC::SCHEMA_FIELD_NOT_EXIST - WHERE条件中的字段不存在
 * - RC::SCHEMA_TABLE_NOT_EXIST - WHERE条件中的表不存在
 * 
 * 数据转换流程：
 * 
 *   ConditionSqlNode (Parser输出)        ComparisonExpr (绑定后)
 *   {                                    {
 *     left_expr: "id"           →          left: FieldExpr(students.id)
 *     comp_op: EQUAL_TO         →          op: EQUAL_TO
 *     right_expr: "1"           →          right: ValueExpr(1)
 *   }                                    }
 */
RC FilterStmt::create(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
    std::vector<ConditionSqlNode> &conditions, FilterStmt *&stmt)
{
  // default_table 没有使用
  RC rc = RC::SUCCESS;
  stmt  = nullptr;

  // ======================== 第1步：将条件转换为表达式 ========================
  // 把 Parser 阶段的 ConditionSqlNode 转换为 ComparisonExpr
  // 
  // 输入：conditions（来自 UpdateSqlNode.conditions 或 SelectSqlNode.conditions）
  // 输出：conditions_exprs（ComparisonExpr 列表）
  //
  // 例如：WHERE id = 1 AND score > 80
  //   → conditions_exprs[0] = ComparisonExpr(id, EQUAL_TO, 1)
  //   → conditions_exprs[1] = ComparisonExpr(score, GREAT_THAN, 80)
  //
  vector<unique_ptr<Expression>> conditions_exprs;
  for (auto &condition : conditions) {
    switch (condition.comp_op) {
      // 【null功能相关】IS 和 IS NOT 用于 NULL 比较
      // 例如：WHERE name IS NULL / WHERE name IS NOT NULL
      case CompOp::COMP_IS:
      case CompOp::COMP_IS_NOT:
      
      // 【子查询相关】EXISTS/NOT EXISTS/IN/NOT IN 用于子查询
      // 例如：WHERE id IN (SELECT id FROM other_table)
      case CompOp::EXISTS:
      case CompOp::NOT_EXISTS:
      case CompOp::IN:
      case CompOp::NOT_IN:
      
      // 常规比较操作
      // =, <>, !=, <, <=, >, >=
      case CompOp::EQUAL_TO:
      case CompOp::LESS_EQUAL:
      case CompOp::NOT_EQUAL:
      case CompOp::LESS_THAN:
      case CompOp::GREAT_EQUAL:
      case CompOp::GREAT_THAN: {
        // 创建比较表达式
        // ComparisonExpr 包含：左操作数、比较符、右操作数
        // hint: 子查询会加入到这其中的一个 expr
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
  // 这样在绑定字段时可以在这些表中查找字段定义
  //
  // 例如：UPDATE students SET age = 25 WHERE id = 1
  //   → tables = {"students": Table*}
  //   → binder_context 知道可以从 students 表查找字段
  //
  BinderContext binder_context;
  for (auto &table : *tables) {
    binder_context.add_table(table.second);
  }
  // 创建表达式绑定器
  ExpressionBinder expression_binder(binder_context);

  vector<unique_ptr<Expression>> bound_conditions;

  // ======================== 第3步：绑定表达式（字段校验） ========================
  // 【select-meta核心】在绑定过程中检查字段是否存在
  //
  // 绑定过程：
  // 1. 解析字段引用（如 "id" 或 "students.id"）
  // 2. 在 binder_context 的表中查找字段
  // 3. 将字段名绑定到具体的 FieldExpr（包含表对象和字段元数据）
  // 4. 如果字段不存在，返回 SCHEMA_FIELD_NOT_EXIST 错误
  //
  // 转换示例：
  //   绑定前：UnboundFieldExpr("id")
  //   绑定后：FieldExpr(table=students, field=id字段元数据)
  //
  auto *tmp_stmt = new FilterStmt();
  for (size_t i = 0; i < conditions.size(); i++) {
    // 保存条件连接符（AND/OR）到 conjunction_types_
    // 用于后续执行时组合多个条件
    // 例如：WHERE id = 1 AND score > 80
    //   → conjunction_types_[0] = AND
    tmp_stmt->conjunction_types_.push_back(conditions[i].conjunction_type);
    
    // 【select-meta关键】绑定表达式
    // expression_binder.bind_expression 会：
    // 1. 递归遍历表达式树
    // 2. 将 UnboundFieldExpr 转换为 FieldExpr
    // 3. 在表中查找字段，如果不存在则返回错误
    RC rc = expression_binder.bind_expression(conditions_exprs[i], bound_conditions);
    if (rc != RC::SUCCESS) {
      // 绑定失败（字段不存在等），释放资源并返回错误
      // 这是 select-meta 功能的关键错误返回点
      delete tmp_stmt;
      LOG_WARN("failed to create filter unit. condition index=%d", i);
      return rc;
    }
  }

  // 清理临时表达式（已经移动到 bound_conditions）
  conditions_exprs.clear();

  // ======================== 第4步：构建FilterStmt对象 ========================
  // 将绑定后的条件转移到 FilterStmt 的 conditions_ 成员中
  //
  // 最终 FilterStmt 结构：
  // tmp_stmt->conditions_[0] = ComparisonExpr(FieldExpr(id), EQUAL_TO, ValueExpr(1))
  // tmp_stmt->conditions_[1] = ComparisonExpr(FieldExpr(score), GREAT_THAN, ValueExpr(80))
  // tmp_stmt->conjunction_types_[0] = AND
  //
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
