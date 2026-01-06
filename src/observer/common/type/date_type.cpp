/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

/**
 * @file date_type.cpp
 * @brief DATE日期类型的实现
 * 
 * 【date功能核心实现文件】
 * 
 * 主要功能：
 * 1. 日期字符串验证（格式：YYYY-MM-DD）
 * 2. 日期字符串与整数的相互转换（存储优化）
 * 3. 日期比较运算
 * 4. 闰年处理
 * 
 * 存储策略：
 * - 日期以整数形式存储，表示从1970-01-01开始的天数（类似Unix Epoch Days）
 * - 这种存储方式：
 *   1. 节省存储空间（4字节整数 vs 10字节字符串）
 *   2. 便于比较大小（直接比较整数）
 *   3. 便于日期计算（加减天数）
 * 
 * 支持的日期范围：
 * - 最小：1970-01-01
 * - 最大：2038-12-31（符合题目要求）
 */

#include <iomanip>

#include "common/lang/comparator.h"
#include "common/lang/sstream.h"
#include "common/log/log.h"
#include "common/type/date_type.h"
#include "common/value.h"
#include "common/lang/limits.h"
#include "common/value.h"
#include "common/types.h"

/**
 * @brief 判断某年是否是闰年
 * 
 * 【date功能辅助函数】
 * 
 * @param year 年份
 * @return true 是闰年
 * @return false 不是闰年
 * 
 * 闰年判断规则：
 * 1. 能被4整除但不能被100整除，或
 * 2. 能被400整除
 * 
 * 示例：
 * - 2000年：能被400整除 -> 闰年
 * - 1900年：能被100整除但不能被400整除 -> 平年
 * - 2024年：能被4整除且不能被100整除 -> 闰年
 */
bool isLeapYear(int year) { return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0); }

/**
 * @brief 计算从 1970-01-01 到给定年初（1月1日）的总天数
 * 
 * 【date功能辅助函数 - 年份转天数】
 * 
 * @param year 目标年份
 * @return int 从1970年1月1日到目标年份1月1日的天数
 * 
 * 算法：
 * 从1970年开始，逐年累加每年的天数
 * - 闰年：366天
 * - 平年：365天
 * 
 * 示例：date2int(1972)
 * = 365(1970) + 365(1971) = 730天
 */
int date2int(int year)
{
  int days = 0;
  // 累加从1970年到目标年份前一年的所有天数
  for (int y = 1970; y < year; ++y) {
    days += isLeapYear(y) ? 366 : 365;
  }
  return days;
}

/**
 * @brief 计算某年某月某日是该年的第几天
 * 
 * 【date功能辅助函数 - 日期转年内天数】
 * 
 * @param year  年份（用于判断闰年）
 * @param month 月份（1-12）
 * @param day   日期（1-31）
 * @return int  该日期是当年的第几天
 * 
 * 算法：
 * 1. 先累加前面所有月份的天数
 * 2. 再加上当月的日期
 * 
 * 示例：dayOfYear(2024, 3, 15)
 * = 31(1月) + 29(2月，闰年) + 15 = 75天
 */
int dayOfYear(int year, int month, int day)
{
  // 每月天数表（平年）
  int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (isLeapYear(year)) {
    daysInMonth[1] = 29;  // 闰年二月有 29 天
  }
  int days = 0;
  // 累加前面所有月份的天数
  for (int m = 1; m < month; ++m) {
    days += daysInMonth[m - 1];
  }
  // 加上当月的日期
  days += day;
  return days;
}

/**
 * @brief 验证日期字符串是否合法（格式：YYYY-MM-DD）
 * 
 * 【date功能核心 - 日期验证】
 * 
 * @param date 日期字符串，格式为 YYYY-MM-DD
 * @return true  日期合法
 * @return false 日期非法
 * 
 * 验证规则：
 * 1. 字符串长度检查（8-10个字符，考虑有无前导零）
 * 2. 格式检查（必须是 YYYY-MM-DD 格式，用'-'分隔）
 * 3. 年份范围检查（1970-2038，符合题目要求）
 * 4. 月份范围检查（1-12）
 * 5. 日期范围检查（根据月份和是否闰年确定最大天数）
 * 
 * 示例：
 * - "2024-02-29" -> true（2024是闰年）
 * - "2023-02-29" -> false（2023是平年，2月只有28天）
 * - "2024-13-01" -> false（月份超出范围）
 */
bool DateType::is_valid(const string date)
{
  // 第1步：检查字符串长度是否合理（考虑有前导 0 和无前导 0 的情况）
  // 最短：1970-1-1（8字符），最长：2038-12-31（10字符）
  if (date.length() < 8 || date.length() > 10) {
    return false;
  }

  int  year, month, day;
  char dash1, dash2;

  // 第2步：使用 istringstream 解析日期字符串
  // 期望格式：年-月-日
  istringstream ss(date);
  if (!(ss >> year >> dash1 >> month >> dash2 >> day) || dash1 != '-' || dash2 != '-') {
    return false;  // 格式不匹配
  }

  // 第3步：检查年份范围（题目要求：1970-2038）
  if (year < 1970 || year > 2038) {
    return false;  // 年份不合法
  }

  // 第4步：检查月份范围（1-12月）
  if (month < 1 || month > 12) {
    return false;  // 月份不合法
  }

  // 第5步：检查日期范围
  // 每月的天数（平年）
  int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  // 闰年2月有29天
  if (month == 2 && isLeapYear(year)) {
    daysInMonth[1] = 29;
  }

  if (day < 1 || day > daysInMonth[month - 1]) {
    return false;  // 天数不合法
  }

  return true;  // 日期合法
}

/**
 * @brief 将日期字符串转换为整数（从1970-01-01开始的天数）
 * 
 * 【date功能核心 - 字符串转整数存储】
 * 
 * @param date 日期字符串，格式为 YYYY-MM-DD
 * @return int 从1970-01-01开始的天数（0表示1970-01-01）
 * 
 * 算法：
 * 1. 解析日期字符串获取年、月、日
 * 2. 计算从1970年到该年年初的天数
 * 3. 加上该日期在当年的天数
 * 4. 减1（因为1月1日是第1天，不是第0天）
 * 
 * 示例：
 * - "1970-01-01" -> 0
 * - "1970-01-02" -> 1
 * - "1971-01-01" -> 365
 */
int DateType::to_int(const string date)
{
  int  year, month, day;
  char dash1, dash2;

  // 解析日期字符串
  istringstream ss(date);
  ss >> year >> dash1 >> month >> dash2 >> day;

  // 计算总天数 = 年初天数 + 年内天数 - 1
  // 减1是因为dayOfYear返回的是"第几天"，而我们需要的是从第0天开始的偏移量
  return date2int(year) + dayOfYear(year, month, day) - 1;
}

/**
 * @brief 将整数天数转换为日期字符串
 * 
 * 【date功能核心 - 整数转字符串显示】
 * 
 * @param date 从1970-01-01开始的天数
 * @return const string 格式化的日期字符串（YYYY-MM-DD）
 * 
 * 算法：
 * 1. 从1970年开始，逐年减去该年的天数，确定年份
 * 2. 从1月开始，逐月减去该月的天数，确定月份
 * 3. 剩余的天数加1就是日期
 * 4. 格式化输出为 YYYY-MM-DD（带前导零）
 * 
 * 示例：
 * - 0    -> "1970-01-01"
 * - 365  -> "1971-01-01"
 * - 730  -> "1972-01-01"
 */
const string DateType::to_str(int date)
{
  int year = 1970;

  // 第1步：确定年份
  // 逐年减去天数，直到剩余天数小于该年的总天数
  while (true) {
    int daysInYear = isLeapYear(year) ? 366 : 365;
    if (date < daysInYear) {
      break;  // 找到了目标年份
    }
    date -= daysInYear;
    year++;
  }

  // 第2步：确定月份和日期
  int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (isLeapYear(year)) {
    daysInMonth[1] = 29;  // 闰年2月29天
  }
  int month = 1;
  // 逐月减去天数，确定月份
  while (date >= daysInMonth[month - 1]) {
    date -= daysInMonth[month - 1];
    month++;
  }
  // 剩余天数加1就是日期（因为日期从1开始，不是从0开始）
  int day = date + 1;

  // 第3步：格式化输出
  // 使用 setw 和 setfill 确保输出格式为 YYYY-MM-DD（带前导零）
  stringstream ss;
  ss << std::setw(4) << std::setfill('0') << year << "-";
  ss << std::setw(2) << std::setfill('0') << month << "-";
  ss << std::setw(2) << std::setfill('0') << day;

  return ss.str();
}

/**
 * @brief 比较两个日期值
 * 
 * 【date功能核心 - 日期比较】
 * 
 * @param left  左操作数（DATE类型）
 * @param right 右操作数（DATE类型）
 * @return int  比较结果：
 *              - 负数：left < right
 *              - 0：left == right
 *              - 正数：left > right
 * 
 * 实现原理：
 * 因为日期以整数形式存储（从1970-01-01的天数），
 * 所以日期比较等价于整数比较，非常高效。
 */
int DateType::compare(const Value &left, const Value &right) const
{
  // 断言检查：确保两个操作数都是DATE类型
  ASSERT(left.attr_type() == AttrType::DATES and right.attr_type() == AttrType::DATES, "left and right type is not both date");
  // 获取日期的整数表示
  float left_val  = left.get_int();
  float right_val = right.get_int();
  // 调用通用整数比较函数
  return common::compare_int((void *)&left_val, (void *)&right_val);
}

/**
 * @brief 从字符串设置日期值
 * 
 * @param val  要设置的Value对象
 * @param data 日期数据的字符串表示（已经是整数形式）
 * @return RC  成功返回SUCCESS，失败返回错误码
 * 
 * 注意：此函数用于从存储的字符串形式恢复日期值，
 * 输入的data应该是整数的字符串表示，而不是"YYYY-MM-DD"格式
 */
RC DateType::set_value_from_str(Value &val, const string &data) const
{
  RC           rc = RC::SUCCESS;
  stringstream deserialize_stream;
  deserialize_stream.clear();  // 清理stream的状态，防止多次解析出现异常
  deserialize_stream.str(data);
  int int_value;
  // 从字符串解析整数
  deserialize_stream >> int_value;
  if (!deserialize_stream || !deserialize_stream.eof()) {
    // 解析失败，类型不匹配
    rc = RC::SCHEMA_FIELD_TYPE_MISMATCH;
  } else {
    // 解析成功，设置日期值
    val.set_date(int_value);
  }
  return rc;
}

/**
 * @brief 将日期值转换为字符串（用于显示）
 * 
 * 【date功能核心 - 日期输出】
 * 
 * @param val    日期Value对象
 * @param result 输出的字符串
 * @return RC    始终返回SUCCESS
 * 
 * 输出格式：YYYY-MM-DD
 * 特殊处理：NULL值显示为"NULL"字符串
 */
RC DateType::to_string(const Value &val, string &result) const
{
  // 【null功能相关】空值展示处理
  if (val.is_null())
  {
    result = "NULL";
    return RC::SUCCESS;
  }
  // 将整数天数转换为日期字符串
  result = to_str(val.value_.int_value_);
  return RC::SUCCESS;
}
