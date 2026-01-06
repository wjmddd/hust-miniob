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
// Created by WangYunlai on 2023/06/28.
//

/**
 * @file value.cpp
 * @brief Value类的实现 - 数据库中值的统一表示
 * 
 * 【null功能和text功能核心实现文件】
 * 
 * 主要功能：
 * 1. 支持多种数据类型：INT, FLOAT, CHAR, DATE, TEXT, BOOLEAN
 * 2. 【null功能】NULL值的存储和处理
 * 3. 【text功能】长文本类型的内存管理
 * 4. 值的类型转换、比较、序列化
 * 
 * Value类的设计：
 * - 使用联合体(union)存储不同类型的值，节省内存
 * - is_null_标志位标识该值是否为NULL
 * - own_data_标志位标识是否拥有动态分配的内存（用于CHARS/TEXT）
 * 
 * NULL值处理策略：
 * - is_null_ = true 表示该值是NULL
 * - 对于不同类型，NULL的实际存储值不同：
 *   - INT/DATE: INT32_MAX
 *   - FLOAT: NaN
 *   - CHARS/TEXT: "NUL\1"
 */

#include <cmath>

#include "common/value.h"

#include "common/lang/comparator.h"
#include "common/lang/exception.h"
#include "common/lang/sstream.h"
#include "common/lang/string.h"
#include "common/log/log.h"

// ======================== 构造函数 ========================

/** @brief 从整数构造Value */
Value::Value(int val) { set_int(val); }

/** @brief 从浮点数构造Value */
Value::Value(float val) { set_float(val); }

/** @brief 从布尔值构造Value */
Value::Value(bool val) { set_boolean(val); }

/**
 * @brief 从字符串构造Value
 * 
 * 【null功能相关】
 * 
 * @param s       字符串指针
 * @param len     字符串长度（0表示自动计算）
 * @param is_null 是否为NULL值
 */
Value::Value(const char *s, int len /*= 0*/, bool is_null /*= false*/) { set_string(s, len, is_null); }

/**
 * @brief 拷贝构造函数
 * 
 * 【text功能相关】
 * 
 * 对于CHARS和TEXT类型，需要深拷贝字符串数据
 * 其他类型直接复制值即可
 */
Value::Value(const Value &other)
{
  this->attr_type_ = other.attr_type_;
  this->length_    = other.length_;
  this->own_data_  = other.own_data_;
  this->is_null_   = other.is_null_;   // 【null功能】复制NULL标志
  switch (this->attr_type_) {
    case AttrType::CHARS: {
      // CHARS类型需要深拷贝字符串
      set_string_from_other(other);
    } break;

    default: {
      // 其他类型直接复制联合体
      this->value_ = other.value_;
    } break;
  }
}

/**
 * @brief 移动构造函数
 * 
 * 【text功能相关 - 性能优化】
 * 
 * 移动语义避免深拷贝，直接转移资源所有权
 * 特别适用于大字符串（TEXT类型）的高效传递
 */
Value::Value(Value &&other)
{
  this->attr_type_ = other.attr_type_;
  this->length_    = other.length_;
  this->own_data_  = other.own_data_;
  this->is_null_   = other.is_null_;   // 【null功能】转移NULL标志
  this->value_     = other.value_;
  // 清除源对象的所有权，防止析构时释放内存
  other.own_data_  = false;
  other.length_    = 0;
}

// ======================== 赋值操作符 ========================

/**
 * @brief 拷贝赋值操作符
 * 
 * 【text功能相关】
 * 
 * 处理TEXTS和CHARS类型时需要深拷贝字符串
 */
Value &Value::operator=(const Value &other)
{
  // 自赋值检查
  if (this == &other) {
    return *this;
  }
  // 先释放当前资源
  reset();
  this->attr_type_ = other.attr_type_;
  this->length_    = other.length_;
  this->own_data_  = other.own_data_;
  this->is_null_   = other.is_null_;   // 【null功能】复制NULL标志
  switch (this->attr_type_) {
    // 【text功能】TEXTS和CHARS类型需要深拷贝字符串内容
    case AttrType::TEXTS:
    case AttrType::CHARS: {
      set_string_from_other(other);
    } break;

    default: {
      // 其他类型直接复制联合体
      this->value_ = other.value_;
    } break;
  }
  return *this;
}

/**
 * @brief 移动赋值操作符
 * 
 * 【text功能相关 - 性能优化】
 * 
 * 转移资源所有权，避免不必要的内存分配和复制
 */
Value &Value::operator=(Value &&other)
{
  if (this == &other) {
    return *this;
  }
  // 先释放当前资源
  reset();
  this->attr_type_ = other.attr_type_;
  this->length_    = other.length_;
  this->own_data_  = other.own_data_;
  this->is_null_   = other.is_null_;   // 【null功能】转移NULL标志
  this->value_     = other.value_;
  // 清除源对象的所有权
  other.own_data_  = false;
  other.length_    = 0;
  return *this;
}

/**
 * @brief 重置Value，释放资源
 * 
 * 【text功能核心 - 内存管理】
 * 
 * 对于TEXTS和CHARS类型，需要释放动态分配的字符串内存
 * 其他类型无需特殊处理
 */
void Value::reset()
{
  switch (attr_type_) {
    // 【text功能】释放TEXT和CHARS类型的动态内存
    case AttrType::TEXTS:
    case AttrType::CHARS:
      if (own_data_ && value_.pointer_value_ != nullptr) {
        delete[] value_.pointer_value_;  // 释放字符串内存
        value_.pointer_value_ = nullptr;
      }
      break;
    default: break;  // 其他类型不需要释放内存
  }

  // 重置所有状态
  attr_type_ = AttrType::UNDEFINED;
  length_    = 0;
  own_data_  = false;
  is_null_   = false;  // 【null功能】重置NULL标志
}

// ======================== 数据设置方法 ========================

/**
 * @brief 从原始二进制数据设置Value
 * 
 * @param data   原始数据指针
 * @param length 数据长度
 * 
 * 根据当前设置的attr_type_解析数据
 * 主要用于从存储层读取记录时恢复Value
 */
void Value::set_data(char *data, int length)
{
  switch (attr_type_) {
    // 【text功能】TEXTS和CHARS统一处理为字符串
    case AttrType::TEXTS:
    case AttrType::CHARS: {
      set_string(data, length);
    } break;
    // 【date功能】DATES类型以整数形式存储
    case AttrType::DATES:
    case AttrType::INTS: {
      value_.int_value_ = *(int *)data;
      length_           = length;
    } break;
    case AttrType::FLOATS: {
      value_.float_value_ = *(float *)data;
      length_             = length;
    } break;
    case AttrType::BOOLEANS: {
      value_.bool_value_ = *(int *)data != 0;
      length_            = length;
    } break;
    default: {
      LOG_WARN("unknown data type: %d", attr_type_);
    } break;
  }
}

/** @brief 设置整数值 */
void Value::set_int(int val)
{
  reset();
  attr_type_        = AttrType::INTS;
  value_.int_value_ = val;
  length_           = sizeof(val);
}

/** @brief 设置浮点数值 */
void Value::set_float(float val)
{
  reset();
  attr_type_          = AttrType::FLOATS;
  value_.float_value_ = val;
  length_             = sizeof(val);
}

/** @brief 设置布尔值 */
void Value::set_boolean(bool val)
{
  reset();
  attr_type_         = AttrType::BOOLEANS;
  value_.bool_value_ = val;
  length_            = sizeof(val);
}

/**
 * @brief 设置日期值
 * 
 * 【date功能核心】
 * 
 * @param val 日期的整数表示（从1970-01-01的天数）
 * 
 * 日期以整数形式存储，便于比较和计算
 */
void Value::set_date(int val)
{
  reset();
  attr_type_        = AttrType::DATES;
  value_.int_value_ = val;  // 日期以整数形式存储
  length_           = sizeof(val);
}

/**
 * @brief 设置字符串值
 * 
 * 【null功能和text功能核心】
 * 
 * @param s       字符串指针
 * @param len     字符串长度（0表示自动计算）
 * @param is_null 是否为NULL值
 * 
 * 处理逻辑：
 * 1. 如果is_null为true，设置为NULL值
 * 2. 否则深拷贝字符串内容
 */
void Value::set_string(const char *s, int len /*= 0*/, bool is_null /*=false */)
{
  reset();
  attr_type_ = AttrType::CHARS;
  
  // 【null功能】处理NULL值
  if (is_null)  // 是 NULL，空值，attr_type_ 暂时设置成 UNDEFINED。内容暂时不初始化
  {
    is_null_   = true;                   // 设置NULL标志
    attr_type_ = AttrType::UNDEFINED;    // 类型设为未定义
    return;
  }
  
  // 【text功能】处理正常字符串
  if (s == nullptr) {
    value_.pointer_value_ = nullptr;
    length_               = 0;
  } else {
    own_data_ = true;  // 标记拥有数据所有权，析构时需要释放
    // 计算字符串长度
    if (len > 0) {
      len = strnlen(s, len);  // 使用指定长度，但不超过实际长度
    } else {
      len = strlen(s);        // 自动计算长度
    }
    // 分配内存并复制字符串
    value_.pointer_value_ = new char[len + 1];  // +1 for '\0'
    length_               = len;
    memcpy(value_.pointer_value_, s, len);
    value_.pointer_value_[len] = '\0';  // 添加字符串结束符
  }
}

/**
 * @brief 设置NULL值的实际存储内容
 * 
 * 【null功能核心 - NULL值的物理存储】
 * 
 * 问题：NULL在逻辑上表示"无值"，但物理存储时必须有一个实际的值
 * 解决方案：为每种类型选择一个"不太可能被正常使用"的特殊值
 * 
 * 各类型的NULL存储值：
 * - TEXTS/CHARS: "NUL\1" （包含不可打印字符\1）
 * - INTS/DATES:  INT32_MAX (2147483647)
 * - FLOATS:      NaN (Not a Number)
 * 
 * 注意：这种方案有局限性，理论上用户可能输入这些特殊值
 * 更好的方案是使用Null Bitmap，但实现更复杂
 */
void Value::set_null_value()
{
  switch (attr_type_) {
    // 【text功能】TEXT和CHARS类型使用特殊字符串标记NULL
    case AttrType::TEXTS:
    case AttrType::CHARS: {
      // 使用 "NUL\1" 作为NULL标记，\1是不可打印字符，用户不太可能输入
      char s[]              = "NUL\1";
      int  len              = strlen(s);
      value_.pointer_value_ = new char[len + 1];
      length_               = len;
      memcpy(value_.pointer_value_, s, len);
      value_.pointer_value_[len] = '\0';
    } break;
    // 【date功能】DATE类型也使用INT32_MAX作为NULL标记
    case AttrType::DATES:
    case AttrType::INTS: {
      // WARN: 这里 INT32_MAX 本应该是一个合法 INT 输入，但是也没有别的办法了。。。
      // 可以考虑使用 Null Bitmap 来解决这个问题
      value_.int_value_ = INT32_MAX;
      length_ = sizeof(INT32_MAX);
    } break;
    case AttrType::FLOATS: {
      // 浮点数使用 NaN (Not a Number) 作为NULL标记
      // NaN有一个特性：NaN != NaN，可用于特殊判断
      value_.float_value_ = std::nanf("");
      length_             = sizeof(std::nanf(""));
    } break;
    // case AttrType::BOOLEANS: { // bool 类型目前没有 UNKNOWN 值，所以无法支持NULL
    default: {
      LOG_WARN("unknown data type: %d", attr_type_);
    } break;
  }
}

/**
 * @brief 设置NULL标志
 * 
 * 【null功能】
 * 
 * @param is_null 是否为NULL
 */
void Value::set_is_null(bool is_null) { is_null_ = is_null; }

/**
 * @brief 从另一个Value设置值
 * 
 * @param value 源Value对象
 * 
 * 根据源Value的类型调用对应的set方法
 */
void Value::set_value(const Value &value)
{
  switch (value.attr_type_) {
    case AttrType::INTS: {
      set_int(value.get_int());
    } break;
    case AttrType::FLOATS: {
      set_float(value.get_float());
    } break;
    // 【date功能】日期类型使用set_date
    case AttrType::DATES: {
      set_date(value.get_int());  // 日期以整数形式存储
    } break;
    // 【text功能】TEXT和CHARS统一处理
    case AttrType::TEXTS:
    case AttrType::CHARS: {
      set_string(value.get_string().c_str());
    } break;
    case AttrType::BOOLEANS: {
      set_boolean(value.get_boolean());
    } break;
    
    default: {
      ASSERT(false, "got an invalid value type");
    } break;
  }
}

/**
 * @brief 从另一个Value深拷贝字符串数据
 * 
 * 【text功能核心 - 字符串深拷贝】
 * 
 * @param other 源Value对象
 * 
 * 仅用于CHARS和TEXTS类型的字符串复制
 * 分配新内存并复制字符串内容
 */
void Value::set_string_from_other(const Value &other)
{
  // 断言检查：确保是字符串类型
  ASSERT(attr_type_ == AttrType::CHARS || attr_type_ == AttrType::TEXTS, "attr type is not CHARS or TEXT");
  // 如果拥有数据且源数据有效，进行深拷贝
  if (own_data_ && other.value_.pointer_value_ != nullptr && length_ != 0) {
    this->value_.pointer_value_ = new char[this->length_ + 1];  // 分配内存
    memcpy(this->value_.pointer_value_, other.value_.pointer_value_, this->length_);  // 复制数据
    this->value_.pointer_value_[this->length_] = '\0';  // 添加结束符
  }
}

/**
 * @brief 获取原始数据指针
 * 
 * @return const char* 数据指针
 * 
 * TEXTS/CHARS返回字符串指针
 * 其他类型返回联合体的地址
 */
const char *Value::data() const
{
  switch (attr_type_) {
    // 【text功能】字符串类型返回指针
    case AttrType::TEXTS:
    case AttrType::CHARS: {
      return value_.pointer_value_;
    } break;
    default: {
      // 其他类型返回联合体的地址
      return (const char *)&value_;
    } break;
  }
}

/**
 * @brief 将Value转换为字符串表示
 * 
 * @return string 字符串表示
 * 
 * 委托给对应类型的DataType实例处理
 * 用于结果输出和调试
 */
string Value::to_string() const
{
  string res;
  RC     rc = DataType::type_instance(this->attr_type_)->to_string(*this, res);
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to convert value to string. type=%s", attr_type_to_string(this->attr_type_));
    return "";
  }
  return res;
}

/**
 * @brief 比较两个Value
 * 
 * @param other 另一个Value
 * @return int  比较结果（负数/0/正数）
 * 
 * 委托给对应类型的DataType实例处理
 */
int Value::compare(const Value &other) const
{
  return DataType::type_instance(this->attr_type_)->compare(*this, other);
}

// ======================== 值获取方法 ========================

/**
 * @brief 获取整数值
 * 
 * @return int 整数值
 * 
 * 支持从多种类型转换：
 * - CHARS: 尝试解析为整数
 * - DATES: 返回日期的整数表示
 * - INTS: 直接返回
 * - FLOATS: 截断小数部分
 * - BOOLEANS: 0或1
 */
int Value::get_int() const
{
  switch (attr_type_) {
    case AttrType::CHARS: {
      // 尝试将字符串解析为整数
      try {
        return (int)(stol(value_.pointer_value_));
      } catch (exception const &ex) {
        LOG_TRACE("failed to convert string to number. s=%s, ex=%s", value_.pointer_value_, ex.what());
        return 0;
      }
    }
    // 【date功能】日期存成整数，直接按整数获取
    case AttrType::DATES:
    case AttrType::INTS: {
      return value_.int_value_;
    }
    case AttrType::FLOATS: {
      return (int)(value_.float_value_);  // 截断小数
    }
    case AttrType::BOOLEANS: {
      return (int)(value_.bool_value_);   // true->1, false->0
    }
    default: {
      LOG_WARN("unknown data type. type=%d", attr_type_);
      return 0;
    }
  }
  return 0;
}

/**
 * @brief 获取浮点数值
 * 
 * @return float 浮点数值
 * 
 * 支持从多种类型转换
 */
float Value::get_float() const
{
  switch (attr_type_) {
    case AttrType::CHARS: {
      // 尝试将字符串解析为浮点数
      try {
        return stof(value_.pointer_value_);
      } catch (exception const &ex) {
        LOG_TRACE("failed to convert string to float. s=%s, ex=%s", value_.pointer_value_, ex.what());
        return 0.0;
      }
    } break;
    case AttrType::INTS: {
      return float(value_.int_value_);
    } break;
    case AttrType::FLOATS: {
      return value_.float_value_;
    } break;
    case AttrType::BOOLEANS: {
      return float(value_.bool_value_);
    } break;
    default: {
      LOG_WARN("unknown data type. type=%d", attr_type_);
      return 0;
    }
  }
  return 0;
}

/**
 * @brief 获取字符串表示
 * 
 * @return string 字符串
 * 
 * 【text功能】用于获取TEXT类型的内容
 */
string Value::get_string() const { return this->to_string(); }

/**
 * @brief 获取布尔值
 * 
 * @return bool 布尔值
 * 
 * 转换规则：
 * - CHARS: 非空且非零字符串为true
 * - INTS: 非零为true
 * - FLOATS: 非零（考虑精度）为true
 * - BOOLEANS: 直接返回
 */
bool Value::get_boolean() const
{
  switch (attr_type_) {
    case AttrType::CHARS: {
      try {
        // 尝试转换为数值，非零为true
        float val = stof(value_.pointer_value_);
        if (val >= EPSILON || val <= -EPSILON) {
          return true;
        }

        int int_val = stol(value_.pointer_value_);
        if (int_val != 0) {
          return true;
        }

        // 字符串非空为true
        return value_.pointer_value_ != nullptr;
      } catch (exception const &ex) {
        LOG_TRACE("failed to convert string to float or integer. s=%s, ex=%s", value_.pointer_value_, ex.what());
        return value_.pointer_value_ != nullptr;
      }
    } break;
    case AttrType::INTS: {
      return value_.int_value_ != 0;
    } break;
    case AttrType::FLOATS: {
      // 浮点数比较需要考虑精度
      float val = value_.float_value_;
      return val >= EPSILON || val <= -EPSILON;
    } break;
    case AttrType::BOOLEANS: {
      return value_.bool_value_;
    } break;
    default: {
      LOG_WARN("unknown data type. type=%d", attr_type_);
      return false;
    }
  }
  return false;
}

/**
 * @brief 判断是否为NULL值
 * 
 * 【null功能核心】
 * 
 * @return bool true表示是NULL值
 */
bool Value::is_null() const { return is_null_; }
