#pragma once

#include "sql/operator/logical_operator.h"
#include "sql/expr/expression.h"
#include "sql/stmt/order_stmt.h"

class Expression;
class OrderUnit;
class OrderStmt;

/**
 * @brief Order逻辑算子
 * @ingroup LogicalOperator
 */
class OrderByLogicalOperator : public LogicalOperator
{
public:
  OrderByLogicalOperator(std::vector<OrderStmt *> stmts)
  {
    std::vector<OrderUnit *> orders;
    for (auto stmt : stmts) {
      orders.push_back(stmt->order_unit());
    }
    orders_.swap(orders);
  }
  OrderByLogicalOperator(std::vector<OrderUnit *> orders) : orders_(orders) {}
  virtual ~OrderByLogicalOperator() = default;

  LogicalOperatorType type() const override { return LogicalOperatorType::ORDER_BY; }

  std::vector<OrderUnit *> &orders() { return orders_; }

private:
  std::vector<OrderUnit *> orders_;
};
