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
// Created by Wangyunlai on 2024/5/31.
//

#pragma once

#include "sql/expr/tuple.h"

/**
 * @brief 组合的Tuple
 * @ingroup Tuple
 * TODO 单元测试
 */
class CompositeTuple : public Tuple
{
public:
  CompositeTuple()          = default;
  virtual ~CompositeTuple() = default;

 /* Tuple *copy() const override
  {
    printf("并没有实现 ProjectTuple 的 copy，返回 NULL。\n");
    return NULL;
  }*/
  /// @brief 删除默认构造函数
  CompositeTuple(const CompositeTuple &) = delete;
  /// @brief 删除默认赋值函数
  CompositeTuple &operator=(const CompositeTuple &) = delete;

  /// @brief 保留移动构造函数
  CompositeTuple(CompositeTuple &&) = default;
  /// @brief 保留移动赋值函数
  CompositeTuple &operator=(CompositeTuple &&) = default;
  Tuple* copy() const override {
    // 1. 创建一个新的 CompositeTuple 对象
    CompositeTuple *new_tuple = new CompositeTuple();

    // 2. 深拷贝 `tuples_` 向量中的每个 Tuple 对象
    for (const auto &tuple : tuples_) {
      // 调用每个 Tuple 对象的 clone 方法并添加到新 CompositeTuple 对象中
      new_tuple->add_tuple(std::unique_ptr<Tuple>(tuple->copy()));
    }

    // 3. 返回新创建的 CompositeTuple 对象
    return new_tuple;
  }

  int cell_num() const override;
  RC  cell_at(int index, Value &cell) const override;
  RC  spec_at(int index, TupleCellSpec &spec) const override;
  RC  find_cell(const TupleCellSpec &spec, Value &cell) const override;

  void   add_tuple(unique_ptr<Tuple> tuple);
  Tuple &tuple_at(size_t index);

private:
  vector<unique_ptr<Tuple>> tuples_;
};
