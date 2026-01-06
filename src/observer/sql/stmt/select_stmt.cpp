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
// Created by Wangyunlai on 2022/6/6.
//

/**
 * @file select_stmt.cpp
 * @brief SELECT语句的语义解析器（Resolver阶段）
 * 
 * 【select-meta功能核心实现文件】
 * 
 * 主要功能：
 * 1. 验证FROM子句中的表是否存在
 * 2. 验证SELECT子句中的字段是否存在于表中
 * 3. 验证WHERE子句中的条件字段是否有效
 * 4. 绑定表达式到具体的表和字段
 * 5. 处理子查询的递归解析
 * 6. 处理GROUP BY和ORDER BY子句
 * 
 * 元数据校验流程：
 *   SQL文本 -> Parser -> SelectSqlNode -> [本文件] -> SelectStmt
 *                                              ↓
 *                                        校验表/字段是否存在
 *                                        如果不存在返回错误码
 */

#include "sql/stmt/select_stmt.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "sql/expr/expression.h"
#include "sql/stmt/filter_stmt.h"
#include "sql/stmt/stmt.h"
#include "storage/db/db.h"
#include "storage/table/table.h"
#include "sql/parser/expression_binder.h"
#include <memory>

using namespace std;
using namespace common;

/**
 * @brief SelectStmt析构函数
 * @details 释放filter_stmt_资源，防止内存泄漏
 */
SelectStmt::~SelectStmt()
{
  if (nullptr != filter_stmt_) {
    delete filter_stmt_;
    filter_stmt_ = nullptr;
  }
}

/**
 * @brief 创建SelectStmt对象（SELECT语句的语义解析核心方法）
 * 
 * 【select-meta功能的核心入口】
 * 
 * @param db                    数据库实例指针
 * @param select_sql            Parser阶段生成的SELECT语法树节点
 * @param stmt                  输出参数，创建成功后指向SelectStmt对象
 * @param loaded_relation_names 已加载的表名列表（用于子查询处理）
 * @return RC                   成功返回SUCCESS，失败返回对应错误码
 * 
 * 元数据校验点：
 * 1. RC::SCHEMA_TABLE_NOT_EXIST - 表不存在
 * 2. RC::SCHEMA_FIELD_NOT_EXIST - 字段不存在（在expression_binder中检查）
 * 3. RC::INVALID_ARGUMENT       - 参数无效
 */
RC SelectStmt::create(Db *db, SelectSqlNode &select_sql, Stmt *&stmt,
                      shared_ptr<vector<string>> loaded_relation_names)
{
  // ======================== 第1步：基本参数验证 ========================
  // 验证数据库指针是否有效
  if (nullptr == db) {
    LOG_WARN("invalid argument. db is null");
    return RC::INVALID_ARGUMENT;
  }
  // 验证SELECT子句是否为空（必须有查询表达式）
  if (select_sql.expressions.empty()) {
    LOG_WARN("invalid argument. select attributes(exprs, technically) is empty");
    return RC::INVALID_ARGUMENT;
  }

  // 初始化已加载表名列表（用于子查询中的外层表引用）
  if (loaded_relation_names == nullptr) 
    loaded_relation_names = std::make_shared<std::vector<string>>();
  // 创建表达式绑定上下文
  BinderContext binder_context; 

  // // 将节点中的 join 添加到 conditions 以及 relations 当中
  // vector<JoinSqlNode> join = std::move(select_sql.join);
  // std::reverse(join.begin(), join.end());
  // for (auto &it : join) {
  //   select_sql.relations.push_back(it.relation);
  //   for (auto& condition : it.conditions)
  //     select_sql.conditions.emplace_back(std::move(condition));
  // }

  // ======================== 第2步：收集并验证FROM子句中的表 ========================
  // 【select-meta核心校验逻辑】检查表是否存在
  vector<Table *>                tables;      // 存储找到的表对象指针
  unordered_map<string, Table *> table_map;   // 表名到表对象的映射，用于后续字段查找

  // 处理子查询场景：首先将外层查询已加载的表添加到table_map中
  // 子查询中可以引用外层查询的表（相关子查询场景）
  // 由于处理子查询是递归进行的，只会由外向内传，所以内层的 sub select 
  // 会额外拥有外层扫到的 table，而外层不会。
  for (auto &rel_name : *loaded_relation_names) {
    // TODO(Soulter): 这里待优化，也就是缓存一下 table 实例的指针。 UPDATE：不能缓存。
    // 【表存在性检查】通过db->find_table检查表是否存在
    Table *table = db->find_table(rel_name.c_str());
    if (nullptr == table) {
      // 表不存在，返回 SCHEMA_TABLE_NOT_EXIST 错误
      LOG_WARN("no such table. db=%s, table_name=%s", db->name(), rel_name.c_str());
      return RC::SCHEMA_TABLE_NOT_EXIST;
    }
    table->set_is_outer_table(true); // 对于当前查询来说，这是来自外层的表。做个标记。
    table_map.insert({rel_name, table});
    LOG_DEBUG("add table from name2alias extraly(sub-query): %s", rel_name.c_str());
  }

  // 【select-meta核心】处理当前SELECT语句的FROM子句中的表
  // 遍历所有关系名（表名），验证每个表是否存在
  for (size_t i = 0; i < select_sql.relations.size(); i++) {
    const char *table_name = select_sql.relations[i].c_str();
    if (nullptr == table_name) {
      LOG_WARN("invalid argument. relation name is null. index=%d", i);
      return RC::INVALID_ARGUMENT;
    }

    // 【表存在性检查】在数据库中查找表
    // db->find_table() 会在数据库的表元数据中查找指定名称的表
    Table *table = db->find_table(table_name);
    if (nullptr == table) {
      // 【select-meta关键返回点】表不存在时返回错误
      // 这是select-meta功能的核心校验：确保查询的表真实存在
      LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
      return RC::SCHEMA_TABLE_NOT_EXIST;
    }

    // 表存在，加入绑定上下文和表列表
    binder_context.add_table(table);          // 添加到表达式绑定上下文
    tables.push_back(table);                   // 添加到表列表
    table_map.insert({table_name, table});     // 添加到表名映射
    loaded_relation_names->push_back(table_name);  // 记录已加载的表名
  }

  // ======================== 第3步：GROUP BY语义检查 ========================
  // 如果有聚合表达式，检查非聚合表达式是否在 group by 语句中
  // 目前只能判断简单的情况，无法判断嵌套的聚合表达式
  // SQL标准要求：SELECT中的非聚合列必须出现在GROUP BY中
  bool has_aggregation = false;
  // 先检查是否存在聚合函数（如COUNT、SUM、AVG等）
  for (unique_ptr<Expression> &expression : select_sql.expressions) {
    if (expression->type() == ExprType::UNBOUND_AGGREGATION) {
      has_aggregation = true;
      break;
    }
  }
  // 如果有聚合函数，验证非聚合列是否都在GROUP BY中
  if (has_aggregation) {
    for (unique_ptr<Expression> &select_expr : select_sql.expressions) {
      // 跳过聚合表达式本身
      if (select_expr->type() == ExprType::UNBOUND_AGGREGATION) {
        continue;
      }

      // 特殊处理：两个聚合表达式的算术运算也是允许的
      if (select_expr->type() == ExprType::ARITHMETIC) {
        ArithmeticExpr *arith_expr = static_cast<ArithmeticExpr *>(select_expr.get());
        if (arith_expr->left() != nullptr && arith_expr->left()->type() == ExprType::UNBOUND_AGGREGATION && arith_expr->right() != nullptr && arith_expr->right()->type() == ExprType::UNBOUND_AGGREGATION) {
          continue;
        }
      }

      // 检查非聚合表达式是否在GROUP BY子句中
      bool found = false;
      for (unique_ptr<Expression> &group_by_expr : select_sql.group_by) {
        if (select_expr->equal(*group_by_expr)) {
          found = true;
          break;
        }
      }
      if (!found) {
        // 【语义错误】非聚合列不在GROUP BY中
        LOG_WARN("non-aggregation expression found in select statement but not in group by statement");
        return RC::INVALID_ARGUMENT;
      }
    }
  }


  // ======================== 第4步：绑定SELECT表达式（字段校验） ========================
  // 【select-meta核心】在这里完成字段存在性检查
  // expression_binder.bind_expression() 会验证字段是否存在于表中
  vector<unique_ptr<Expression>> bound_expressions;
  ExpressionBinder               expression_binder(binder_context);

  // 遍历SELECT子句中的每个表达式并绑定
  // 绑定过程会检查字段是否存在，不存在则返回 SCHEMA_FIELD_NOT_EXIST
  for (unique_ptr<Expression> &expression : select_sql.expressions) {
    RC rc = expression_binder.bind_expression(expression, bound_expressions);
    if (OB_FAIL(rc)) {
      // 【select-meta关键返回点】字段不存在等错误在这里返回
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  // 绑定GROUP BY表达式
  vector<unique_ptr<Expression>> group_by_expressions;
  for (unique_ptr<Expression> &expression : select_sql.group_by) {
    RC rc = expression_binder.bind_expression(expression, group_by_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  // 设置默认表（单表查询时使用，简化字段查找）
  Table *default_table = nullptr;
  if (tables.size() == 1) {
    default_table = tables[0];
  }

  // ======================== 第5步：处理ORDER BY子句 ========================
  // 解析并验证ORDER BY中的字段
  std::vector<OrderStmt *> order_by;
  for (size_t i = 0; i < select_sql.order_by.size(); i++) {
    RC         rc         = RC::SUCCESS;
    OrderStmt *order_stmt = nullptr;
    // OrderStmt::create 内部会检查排序字段是否存在
    rc                    = OrderStmt::create(db, default_table, &table_map, select_sql.order_by[i], order_stmt);
    if (rc != RC::SUCCESS) {
      return rc;
    }
    order_by.push_back(order_stmt);
  }

  // ======================== 第6步：处理子查询（递归解析） ========================
  // 子查询，遍历 conditions 中的表达式，（递归）创建对应的 stmt。
  // 这个 for 会将所有的子查询的 stmt 都创建好，放到 SubqueryExpr 中
  // 子查询的元数据校验通过递归调用 SelectStmt::create 完成
  for (auto &condition : select_sql.conditions) {
    // 处理左侧子查询（exists/not exists 可能会使得 left_expr 为空）
    if (condition.left_expr != nullptr && condition.left_expr->type() == ExprType::SUB_QUERY) {
      SubqueryExpr *subquery_expr = static_cast<SubqueryExpr *>(condition.left_expr.get());
      Stmt         *stmt          = nullptr;
      // 【递归调用】对子查询进行同样的元数据校验
      // loaded_relation_names 传递外层表信息，支持相关子查询
      RC            rc            = SelectStmt::create(
        db, 
        subquery_expr->sub_query_sn().selection, 
        stmt, 
        loaded_relation_names
      );
      if (rc != RC::SUCCESS) {
        LOG_WARN("cannot construct subquery stmt");
        return rc;
      }
      // 检查子查询的合法性：子查询的查询的属性只能有一个
      RC rc_ = check_sub_select_legal(db, subquery_expr->sub_query_sn());
      if (rc_ != RC::SUCCESS) {
        return rc_;
      }
      subquery_expr->set_stmt(unique_ptr<SelectStmt>(static_cast<SelectStmt *>(stmt)));
    }
    // 处理右侧子查询
    if (condition.right_expr != nullptr && condition.right_expr->type() == ExprType::SUB_QUERY) {
      SubqueryExpr *subquery_expr = static_cast<SubqueryExpr *>(condition.right_expr.get());
      Stmt         *stmt          = nullptr;
      // 【递归调用】对子查询进行同样的元数据校验
      RC            rc            = SelectStmt::create(
        db,
        subquery_expr->sub_query_sn().selection, 
        stmt,
        loaded_relation_names
      );
      if (rc != RC::SUCCESS) {
        LOG_WARN("cannot construct subquery stmt");
        return rc;
      }
      // 检查子查询的合法性：子查询的查询的属性只能有一个
      RC rc_ = check_sub_select_legal(db, subquery_expr->sub_query_sn());
      if (rc_ != RC::SUCCESS) {
        return rc_;
      }
      subquery_expr->set_stmt(unique_ptr<SelectStmt>(static_cast<SelectStmt *>(stmt)));
    }
  }

  

  // ======================== 第7步：创建WHERE子句过滤语句 ========================
  // 【select-meta】FilterStmt::create 内部会验证WHERE条件中的字段是否存在
  FilterStmt *filter_stmt = nullptr;
  RC          rc          = FilterStmt::create(db, default_table, &table_map, select_sql.conditions, filter_stmt);
  if (rc != RC::SUCCESS) {
    // WHERE子句中的字段不存在等错误在这里返回
    LOG_WARN("cannot construct filter stmt");
    return rc;
  }




  // ======================== 第8步：构建SelectStmt对象 ========================
  // 所有校验通过，创建最终的SelectStmt对象
  SelectStmt *select_stmt = new SelectStmt();

  // 使用swap高效地转移数据所有权，避免深拷贝
  select_stmt->tables_.swap(tables);                        // 设置涉及的表
  select_stmt->query_expressions_.swap(bound_expressions);  // 设置查询表达式
  select_stmt->filter_stmt_ = filter_stmt;                  // 设置WHERE过滤条件
  select_stmt->order_by_.swap(order_by);                    // 设置ORDER BY
  select_stmt->group_by_.swap(group_by_expressions);        // 设置GROUP BY
  stmt = select_stmt;
  return RC::SUCCESS;
}
