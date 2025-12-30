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
// Created by Wangyunlai on 2021/5/14.
//

#pragma once

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "common/log/log.h"
#include "sql/expr/expression.h"
#include "sql/expr/tuple_cell.h"
#include "sql/parser/parse.h"
#include "common/value.h"
#include "storage/record/record.h"

class Table;

/**
 * @defgroup Tuple
 * @brief Tuple 元组，表示一行数据，当前返回客户端时使用
 * @details
 * tuple是一种可以嵌套的数据结构。
 * 比如select t1.a+t2.b from t1, t2;
 * 需要使用下面的结构表示：
 * @code {.cpp}
 *  Project(t1.a+t2.b)
 *        |
 *      Joined
 *      /     \
 *   Row(t1) Row(t2)
 * @endcode
 * TODO 一个类拆分成一个文件，并放到单独的目录中
 */

/**
 * @brief 元组的结构，包含哪些字段(这里成为Cell)，每个字段的说明
 * @ingroup Tuple
 */
class TupleSchema
{
public:
  void append_cell(const TupleCellSpec &cell) { cells_.push_back(cell); }
  void append_cell(const char *table, const char *field) { append_cell(TupleCellSpec(table, field)); }
  void append_cell(const char *alias) { append_cell(TupleCellSpec(alias)); }
  int  cell_num() const { return static_cast<int>(cells_.size()); }

  const TupleCellSpec &cell_at(int i) const { return cells_[i]; }

private:
  vector<TupleCellSpec> cells_;
};

/**
 * @brief 元组的抽象描述
 * @ingroup Tuple
 */
class Tuple
{
public:
  Tuple()                     = default;
  virtual ~Tuple()            = default;
  virtual Tuple *copy() const = 0;

  /**
   * @brief 获取元组中的Cell的个数
   * @details 个数应该与tuple_schema一致
   */
  virtual int cell_num() const = 0;

  /**
   * @brief 获取指定位置的Cell
   *
   * @param index 位置
   * @param[out] cell  返回的Cell
   */
  virtual RC cell_at(int index, Value &cell) const = 0;

  virtual RC spec_at(int index, TupleCellSpec &spec) const = 0;

  /**
   * @brief 根据cell的描述，获取cell的值
   *
   * @param spec cell的描述
   * @param[out] cell 返回的cell
   */
  virtual RC find_cell(const TupleCellSpec &spec, Value &cell) const = 0;

  virtual string to_string() const
  {
    string    str;
    const int cell_num = this->cell_num();
    for (int i = 0; i < cell_num - 1; i++) {
      Value cell;
      cell_at(i, cell);
      str += cell.to_string();
      str += ", ";
    }

    if (cell_num > 0) {
      Value cell;
      cell_at(cell_num - 1, cell);
      str += cell.to_string();
    }
    return str;
  }

  virtual RC compare(const Tuple &other, int &result) const
  {
    RC rc = RC::SUCCESS;

    const int this_cell_num  = this->cell_num();
    const int other_cell_num = other.cell_num();
    if (this_cell_num < other_cell_num) {
      result = -1;
      return rc;
    }
    if (this_cell_num > other_cell_num) {
      result = 1;
      return rc;
    }

    Value this_value;
    Value other_value;
    for (int i = 0; i < this_cell_num; i++) {
      rc = this->cell_at(i, this_value);
      if (OB_FAIL(rc)) {
        return rc;
      }

      rc = other.cell_at(i, other_value);
      if (OB_FAIL(rc)) {
        return rc;
      }

      result = this_value.compare(other_value);
      if (0 != result) {
        return rc;
      }
    }

    result = 0;
    return rc;
  }
  void set_rid(const RID &rid) { rid_ = rid; }
  void set_table_name(const std::string &table_name) { table_name_ = table_name; }
  RID  raw_rid() const { return rid_; }
  const std::string &raw_table_name() const { return table_name_; }

  std::string table_alias_;

protected:
  RID rid_;
  std::string table_name_;
};

/**
 * @brief 一行数据的元组
 * @ingroup Tuple
 * @details 直接就是获取表中的一条记录
 */
class RowTuple : public Tuple
{
public:
  RowTuple() = default;
  RowTuple(const RowTuple &other)
  {
    if (other.record_)
      record_ = new Record(*(other.record_));
    table_ = other.table_;
    for (auto fieldExpr : other.speces_) {
      if (fieldExpr) {
        speces_.push_back(new FieldExpr(*fieldExpr));
      } else {
        speces_.push_back(nullptr);
      }
    }
  }
  virtual ~RowTuple()
  {
    for (FieldExpr *spec : speces_) {
      delete spec;
      spec = nullptr;  // 删除后将指针置为 nullptr，避免悬挂指针
    }
    speces_.clear();
  }

  Tuple *copy() const override { return new RowTuple(*this); }

  void set_record(Record *record) { this->record_ = record; }

  void set_schema(const Table *table, const vector<FieldMeta> *fields)
  {
    table_ = table;
    // fix:join当中会多次调用右表的open,open当中会调用set_scheme，从而导致tuple当中会存储
    // 很多无意义的field和value，因此需要先clear掉
    for (FieldExpr *spec : speces_) {
      delete spec;
    }
    this->speces_.clear();
    this->speces_.reserve(fields->size());
    for (const FieldMeta &field : *fields) {
      speces_.push_back(new FieldExpr(table, &field));
    }
  }

  int cell_num() const override { return speces_.size(); }

  RC cell_at(int index, Value &cell) const override
  {
    if (index < 0 || index >= static_cast<int>(speces_.size())) {
      LOG_WARN("invalid argument. index=%d", index);
      return RC::INVALID_ARGUMENT;
    }

    FieldExpr       *field_expr = speces_[index];
    const FieldMeta *field_meta = field_expr->field().meta();
    cell.set_type(field_meta->type());
    if (field_meta->type() == AttrType::TEXTS) {
      // 因为 TEXT 使用了不同的存储方式，所以这里要特殊处理
      int          index   = *(int *)(this->record_->data() + field_meta->offset());
      const string content = table_->get_text_attribute(index);
      cell.set_data(content.c_str(), 0);
    } else {
      cell.set_data(this->record_->data() + field_meta->offset(), field_meta->len());
    }
    // 根据内容恢复空值
    switch (field_meta->type()) {
      case AttrType::INTS:
      case AttrType::DATES:
        if (cell.get_int() == INT32_MAX) {
          cell.set_is_null(true);
        } else {
          cell.set_is_null(false);
        }
        break;
      case AttrType::FLOATS:
        if (cell.get_float() == std::nanf("")) {
          cell.set_is_null(true);
        } else {
          cell.set_is_null(false);
        }
        break;
      case AttrType::TEXTS:
      case AttrType::CHARS:
        if (strcmp(cell.get_string().c_str(), "NUL\1") == 0) {
          cell.set_is_null(true);
        } else {
          cell.set_is_null(false);
        }
        break;
      default: break;
    }
    return RC::SUCCESS;
  }

  RC spec_at(int index, TupleCellSpec &spec) const override
  {
    const Field &field = speces_[index]->field();
    spec               = TupleCellSpec(table_->name(), field.field_name());
    return RC::SUCCESS;
  }

  RC find_cell(const TupleCellSpec &spec, Value &cell) const override
  {
    const char *table_name = spec.table_name();
    const char *field_name = spec.field_name();
    if (0 != strcmp(table_name, table_->name())) {
      return RC::NOTFOUND;
    }

    for (size_t i = 0; i < speces_.size(); ++i) {
      const FieldExpr *field_expr = speces_[i];
      const Field     &field      = field_expr->field();
      if (0 == strcmp(field_name, field.field_name())) {
        return cell_at(i, cell);
      }
    }
    return RC::NOTFOUND;
  }

#if 0
  RC cell_spec_at(int index, const TupleCellSpec *&spec) const override
  {
    if (index < 0 || index >= static_cast<int>(speces_.size())) {
      LOG_WARN("invalid argument. index=%d", index);
      return RC::INVALID_ARGUMENT;
    }
    spec = speces_[index];
    return RC::SUCCESS;
  }
#endif

  Record &record() { return *record_; }

  const Record &record() const { return *record_; }

private:
  Record             *record_ = nullptr;
  const Table        *table_  = nullptr;
  vector<FieldExpr *> speces_;
};

/**
 * @brief 从一行数据中，选择部分字段组成的元组，也就是投影操作
 * @ingroup Tuple
 * @details 一般在select语句中使用。
 * 投影也可以是很复杂的操作，比如某些字段需要做类型转换、重命名、表达式运算、函数计算等。
 */
class ProjectTuple : public Tuple
{
public:
  ProjectTuple() = default;

  virtual ~ProjectTuple()
  {
    for (TupleCellSpec *spec : speces_) {
      delete spec;
    }
    speces_.clear();
  }

  Tuple *copy() const override
  {
    //printf("并没有实现 ProjectTuple 的 copy，返回 NULL。\n");z`
    //return NULL;
    return new ProjectTuple(*this);
  }

  void set_tuple(Tuple *tuple)
  {
    this->tuple_ = tuple;
  }
  void add_cell_spec(TupleCellSpec *spec)
  {
    speces_.push_back(spec);
  }

  int cell_num() const override { return speces_.size(); }
  RC spec_at(int index, TupleCellSpec &spec) const override
  {
    if (index < 0 || index >= static_cast<int>(speces_.size())) {
      return RC::NOTFOUND;
    }
    spec = *speces_[index];
    return RC::SUCCESS;
  }
  RC cell_at(int index, Value &cell) const override
  {
    if (index < 0 || index >= static_cast<int>(speces_.size())) {
      return RC::INTERNAL;
    }
    if (tuple_ == nullptr) {
      return RC::INTERNAL;
    }

    const TupleCellSpec *spec = speces_[index];
    return tuple_->find_cell(*spec, cell);
  }

  RC find_cell(const TupleCellSpec &spec, Value &cell) const override { return tuple_->find_cell(spec, cell); }

#if 0
  RC cell_spec_at(int index, const TupleCellSpec *&spec) const override
  {
    if (index < 0 || index >= static_cast<int>(speces_.size())) {
      return RC::NOTFOUND;
    }
    spec = speces_[index];
    return RC::SUCCESS;
  }
#endif
private:
  //vector<unique_ptr<Expression>> expressions_;
  std::vector<TupleCellSpec *> speces_;
  Tuple                         *tuple_ = nullptr;
};

/**
 * @brief 一些常量值组成的Tuple
 * @ingroup Tuple
 * TODO 使用单独文件
 */
class ValueListTuple : public Tuple
{
public:
  ValueListTuple()          = default;
  virtual ~ValueListTuple() = default;

  Tuple *copy() const override { return new ValueListTuple(*this); }

  void set_names(const vector<TupleCellSpec> &specs) { specs_ = specs; }
  void set_cells(const vector<Value> &cells) { cells_ = cells; }

  virtual int cell_num() const override { return static_cast<int>(cells_.size()); }

  virtual RC cell_at(int index, Value &cell) const override
  {
    if (index < 0 || index >= cell_num()) {
      return RC::NOTFOUND;
    }

    cell = cells_[index];
    return RC::SUCCESS;
  }

  RC spec_at(int index, TupleCellSpec &spec) const override
  {
    if (index < 0 || index >= cell_num()) {
      return RC::NOTFOUND;
    }

    spec = specs_[index];
    return RC::SUCCESS;
  }

  virtual RC find_cell(const TupleCellSpec &spec, Value &cell) const override
  {
    if (specs_.size() != cells_.size()) {
      return RC::INTERNAL;
    }
    for (size_t i = 0; i < specs_.size(); i++) {
      if (spec.alias() && specs_[i].alias() && 0 == strcmp(spec.alias(), specs_[i].alias())) {
        return cell_at(i, cell);
      }
    }
    return RC::EMPTY;
  }

  static RC make(const Tuple &tuple, ValueListTuple &value_list)
  {
    const int cell_num = tuple.cell_num();
    for (int i = 0; i < cell_num; i++) {
      Value cell;
      RC    rc = tuple.cell_at(i, cell);
      if (OB_FAIL(rc)) {
        return rc;
      }

      TupleCellSpec spec;
      rc = tuple.spec_at(i, spec);
      if (OB_FAIL(rc)) {
        return rc;
      }

      value_list.cells_.push_back(cell);
      value_list.specs_.push_back(spec);
    }
    return RC::SUCCESS;
  }

private:
  vector<Value>         cells_;
  vector<TupleCellSpec> specs_;
};

/**
 * @brief 将两个tuple合并为一个tuple
 * @ingroup Tuple
 * @details 在join算子中使用
 * TODO replace with composite tuple
 */
class JoinedTuple : public Tuple
{
public:
  JoinedTuple() = default;
  JoinedTuple(const JoinedTuple &other)
  {
    if (other.left_) {
      left_ = other.left_->copy();
    }

    if (other.right_) {
      right_ = other.right_->copy();
    }
  }
  virtual ~JoinedTuple() = default;

  Tuple *copy() const override { return new JoinedTuple(*this); }

  void set_left(Tuple *left) { left_ = left; }
  void set_right(Tuple *right) { right_ = right; }

  int cell_num() const override { return left_->cell_num() + right_->cell_num(); }

  RC cell_at(int index, Value &value) const override
  {
    const int left_cell_num = left_->cell_num();
    if (index >= 0 && index < left_cell_num) {
      return left_->cell_at(index, value);
    }

    if (index >= left_cell_num && index < left_cell_num + right_->cell_num()) {
      return right_->cell_at(index - left_cell_num, value);
    }

    return RC::NOTFOUND;
  }

  RC spec_at(int index, TupleCellSpec &spec) const override
  {
    const int left_cell_num = left_->cell_num();
    if (index >= 0 && index < left_cell_num) {
      return left_->spec_at(index, spec);
    }

    if (index >= left_cell_num && index < left_cell_num + right_->cell_num()) {
      return right_->spec_at(index - left_cell_num, spec);
    }

    return RC::NOTFOUND;
  }

  RC find_cell(const TupleCellSpec &spec, Value &value) const override
  {
    RC rc = left_->find_cell(spec, value);
    if (rc == RC::SUCCESS || rc != RC::NOTFOUND) {
      return rc;
    }

    return right_->find_cell(spec, value);
  }

private:
  Tuple *left_  = nullptr;
  Tuple *right_ = nullptr;
};
