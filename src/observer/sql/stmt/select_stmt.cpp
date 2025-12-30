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

SelectStmt::~SelectStmt()
{
  if (nullptr != filter_stmt_) {
    delete filter_stmt_;
    filter_stmt_ = nullptr;
  }
}

RC SelectStmt::create(Db *db, SelectSqlNode &select_sql, Stmt *&stmt,
                      shared_ptr<vector<string>> loaded_relation_names)
{
  if (nullptr == db) {
    LOG_WARN("invalid argument. db is null");
    return RC::INVALID_ARGUMENT;
  }
  if (select_sql.expressions.empty()) {
    LOG_WARN("invalid argument. select attributes(exprs, technically) is empty");
    return RC::INVALID_ARGUMENT;
  }

  if (loaded_relation_names == nullptr) 
    loaded_relation_names = std::make_shared<std::vector<string>>();
  BinderContext binder_context; 

  // // 将节点中的 join 添加到 conditions 以及 relations 当中
  // vector<JoinSqlNode> join = std::move(select_sql.join);
  // std::reverse(join.begin(), join.end());
  // for (auto &it : join) {
  //   select_sql.relations.push_back(it.relation);
  //   for (auto& condition : it.conditions)
  //     select_sql.conditions.emplace_back(std::move(condition));
  // }

  // collect tables in `from` statement
  vector<Table *>                tables;
  unordered_map<string, Table *> table_map;

  // 首先将 loaded_relation_names 中的表名添加到 table_map 中
  // 由于处理子查询是递归进行的，只会由外向内传，所以内层的 sub select 
  // 会额外拥有外层扫到的 table，而外层不会。
  for (auto &rel_name : *loaded_relation_names) {
    // TODO(Soulter): 这里待优化，也就是缓存一下 table 实例的指针。 UPDATE：不能缓存。
    Table *table = db->find_table(rel_name.c_str());
    if (nullptr == table) {
      LOG_WARN("no such table. db=%s, table_name=%s", db->name(), rel_name.c_str());
      return RC::SCHEMA_TABLE_NOT_EXIST;
    }
    table->set_is_outer_table(true); // 对于当前查询来说，这是来自外层的表。做个标记。
    table_map.insert({rel_name, table});
    LOG_DEBUG("add table from name2alias extraly(sub-query): %s", rel_name.c_str());
  }

  // 然后才是处理 select 语句中的 from 语句
  for (size_t i = 0; i < select_sql.relations.size(); i++) {
    const char *table_name = select_sql.relations[i].c_str();
    if (nullptr == table_name) {
      LOG_WARN("invalid argument. relation name is null. index=%d", i);
      return RC::INVALID_ARGUMENT;
    }

    Table *table = db->find_table(table_name);
    if (nullptr == table) {
      LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
      return RC::SCHEMA_TABLE_NOT_EXIST;
    }

    binder_context.add_table(table);
    tables.push_back(table);
    table_map.insert({table_name, table});
    loaded_relation_names->push_back(table_name);
  }

   // 如果有聚合表达式，检查非聚合表达式是否在 group by 语句中
  // 目前只能判断简单的情况，无法判断嵌套的聚合表达式
  bool has_aggregation = false;
  for (unique_ptr<Expression> &expression : select_sql.expressions) {
    if (expression->type() == ExprType::UNBOUND_AGGREGATION) {
      has_aggregation = true;
      break;
    }
  }
  if (has_aggregation) {
    for (unique_ptr<Expression> &select_expr : select_sql.expressions) {
      if (select_expr->type() == ExprType::UNBOUND_AGGREGATION) {
        continue;
      }

      if (select_expr->type() == ExprType::ARITHMETIC) {
        ArithmeticExpr *arith_expr = static_cast<ArithmeticExpr *>(select_expr.get());
        if (arith_expr->left() != nullptr && arith_expr->left()->type() == ExprType::UNBOUND_AGGREGATION && arith_expr->right() != nullptr && arith_expr->right()->type() == ExprType::UNBOUND_AGGREGATION) {
          continue;
        }
      }


      bool found = false;
      for (unique_ptr<Expression> &group_by_expr : select_sql.group_by) {
        if (select_expr->equal(*group_by_expr)) {
          found = true;
          break;
        }
      }
      if (!found) {
        LOG_WARN("non-aggregation expression found in select statement but not in group by statement");
        return RC::INVALID_ARGUMENT;
      }
    }
  }


  // collect query fields in `select` statement
  vector<unique_ptr<Expression>> bound_expressions;
  ExpressionBinder               expression_binder(binder_context);

  for (unique_ptr<Expression> &expression : select_sql.expressions) {
    RC rc = expression_binder.bind_expression(expression, bound_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  vector<unique_ptr<Expression>> group_by_expressions;
  for (unique_ptr<Expression> &expression : select_sql.group_by) {
    RC rc = expression_binder.bind_expression(expression, group_by_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  Table *default_table = nullptr;
  if (tables.size() == 1) {
    default_table = tables[0];
  }

  std::vector<OrderStmt *> order_by;
  for (size_t i = 0; i < select_sql.order_by.size(); i++) {
    RC         rc         = RC::SUCCESS;
    OrderStmt *order_stmt = nullptr;
    rc                    = OrderStmt::create(db, default_table, &table_map, select_sql.order_by[i], order_stmt);
    if (rc != RC::SUCCESS) {
      return rc;
    }
    order_by.push_back(order_stmt);
  }

  // 子查询，遍历 conditions 中的表达式，（递归）创建对应的 stmt。
  // 这个 for 会将所有的子查询的 stmt 都创建好，放到 SubqueryExpr 中
  for (auto &condition : select_sql.conditions) {
    // exists/not exists 可能会使得 left_expr 为空
    if (condition.left_expr != nullptr && condition.left_expr->type() == ExprType::SUB_QUERY) {
      SubqueryExpr *subquery_expr = static_cast<SubqueryExpr *>(condition.left_expr.get());
      Stmt         *stmt          = nullptr;
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
      //检查子查询的合法性：子查询的查询的属性只能有一个
      RC rc_ = check_sub_select_legal(db, subquery_expr->sub_query_sn());
      if (rc_ != RC::SUCCESS) {
        return rc_;
      }
      subquery_expr->set_stmt(unique_ptr<SelectStmt>(static_cast<SelectStmt *>(stmt)));
    }
    if (condition.right_expr != nullptr && condition.right_expr->type() == ExprType::SUB_QUERY) {
      SubqueryExpr *subquery_expr = static_cast<SubqueryExpr *>(condition.right_expr.get());
      Stmt         *stmt          = nullptr;
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
      //检查子查询的合法性：子查询的查询的属性只能有一个
      RC rc_ = check_sub_select_legal(db, subquery_expr->sub_query_sn());
      if (rc_ != RC::SUCCESS) {
        return rc_;
      }
      subquery_expr->set_stmt(unique_ptr<SelectStmt>(static_cast<SelectStmt *>(stmt)));
    }
  }

  

   // create filter statement in `where` statement
  FilterStmt *filter_stmt = nullptr;
  RC          rc          = FilterStmt::create(db, default_table, &table_map, select_sql.conditions, filter_stmt);
  if (rc != RC::SUCCESS) {
    LOG_WARN("cannot construct filter stmt");
    return rc;
  }




  // everything alright
  SelectStmt *select_stmt = new SelectStmt();

  select_stmt->tables_.swap(tables);
  select_stmt->query_expressions_.swap(bound_expressions);
  select_stmt->filter_stmt_ = filter_stmt;
  select_stmt->order_by_.swap(order_by);
  select_stmt->group_by_.swap(group_by_expressions);
  stmt = select_stmt;
  return RC::SUCCESS;
}
