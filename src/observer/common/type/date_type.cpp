/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include <iomanip>

#include "common/lang/comparator.h"
#include "common/lang/sstream.h"
#include "common/log/log.h"
#include "common/type/date_type.h"
#include "common/value.h"
#include "common/lang/limits.h"
#include "common/value.h"
#include "common/types.h"

// 判断某年是否是闰年
bool isLeapYear(int year) { return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0); }

// 计算从 1970-01-01 到给定年初的总天数
int date2int(int year)
{
  int days = 0;
  for (int y = 1970; y < year; ++y) {
    days += isLeapYear(y) ? 366 : 365;
  }
  return days;
}

// 计算某年某月某日是该年的第几天
int dayOfYear(int year, int month, int day)
{
  int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (isLeapYear(year)) {
    daysInMonth[1] = 29;  // 闰年二月有 29 天
  }
  int days = 0;
  for (int m = 1; m < month; ++m) {
    days += daysInMonth[m - 1];
  }
  days += day;
  return days;
}

// 验证 YYYY-MM-DD 格式的日期是否合法
bool DateType::is_valid(const string date)
{
  // 检查字符串长度是否合理（考虑有前导 0 和无前导 0 的情况）
  if (date.length() < 8 || date.length() > 10) {
    return false;
  }

  int  year, month, day;
  char dash1, dash2;

  // 使用 istringstream 解析日期
  istringstream ss(date);
  if (!(ss >> year >> dash1 >> month >> dash2 >> day) || dash1 != '-' || dash2 != '-') {
    return false;  // 格式不匹配
  }

  // 检查年、月、日范围
  if (year < 1970 || year > 2038) {
    return false;  // 年份不合法
  }
  if (month < 1 || month > 12) {
    return false;  // 月份不合法
  }

  // 每月的天数
  int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && isLeapYear(year)) {
    daysInMonth[1] = 29;
  }

  if (day < 1 || day > daysInMonth[month - 1]) {
    return false;  // 天数不合法
  }

  return true;  // 日期合法
}

int DateType::to_int(const string date)
{
  int  year, month, day;
  char dash1, dash2;

  // 解析日期字符串
  istringstream ss(date);
  ss >> year >> dash1 >> month >> dash2 >> day;

  // 计算总天数
  return date2int(year) + dayOfYear(year, month, day) - 1;
}

const string DateType::to_str(int date)
{
  int year = 1970;

  // 计算年份
  while (true) {
    int daysInYear = isLeapYear(year) ? 366 : 365;
    if (date < daysInYear) {
      break;
    }
    date -= daysInYear;
    year++;
  }

  // 计算月份和日期
  int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (isLeapYear(year)) {
    daysInMonth[1] = 29;
  }
  int month = 1;
  while (date >= daysInMonth[month - 1]) {
    date -= daysInMonth[month - 1];
    month++;
  }
  int day = date + 1;

  // 格式化日期字符串
  stringstream ss;
  ss << std::setw(4) << std::setfill('0') << year << "-";
  ss << std::setw(2) << std::setfill('0') << month << "-";
  ss << std::setw(2) << std::setfill('0') << day;

  return ss.str();
}

int DateType::compare(const Value &left, const Value &right) const
{
  ASSERT(left.attr_type() == AttrType::DATES and right.attr_type() == AttrType::DATES, "left and right type is not both date");
  float left_val  = left.get_int();
  float right_val = right.get_int();
  return common::compare_int((void *)&left_val, (void *)&right_val);
}

RC DateType::set_value_from_str(Value &val, const string &data) const
{
  RC           rc = RC::SUCCESS;
  stringstream deserialize_stream;
  deserialize_stream.clear();  // 清理stream的状态，防止多次解析出现异常
  deserialize_stream.str(data);
  int int_value;
  deserialize_stream >> int_value;
  if (!deserialize_stream || !deserialize_stream.eof()) {
    rc = RC::SCHEMA_FIELD_TYPE_MISMATCH;
  } else {
    val.set_date(int_value);
  }
  return rc;
}

RC DateType::to_string(const Value &val, string &result) const
{
  if (val.is_null()) // 空值展示
  {
    result = "NULL";
    return RC::SUCCESS;
  }
  result = to_str(val.value_.int_value_);
  return RC::SUCCESS;
}
