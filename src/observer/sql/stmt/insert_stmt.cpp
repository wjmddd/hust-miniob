/* Copyright (c) 2021OceanBase and/or its affiliates. All rights reserved.
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
 * @file insert_stmt.cpp
 * @brief INSERT语句的语义解析器（Resolver阶段）
 * 
 * 【insert功能和null功能相关实现】
 * 
 * 主要功能：
 * 1. 验证INSERT语句中的表是否存在
 * 2. 验证插入值的数量与表字段数量是否匹配
 * 3. 验证插入值的类型与字段类型是否匹配（支持类型转换）
 * 4. 【null功能】验证NULL值是否可以插入到目标字段
 * 5. 【insert多行功能】支持一条INSERT插入多行数据
 * 
 * 处理流程：
 *   INSERT语法树(InsertSqlNode) -> 表存在性检查 -> 字段数量检查 
 *                                      -> 类型检查/NULL检查 -> InsertStmt
 */

#include "sql/stmt/insert_stmt.h"
#include "common/log/log.h"
#include "storage/db/db.h"
#include "storage/table/table.h"
#include "common/type/date_type.h"

class DateType;

/**
 * @brief InsertStmt构造函数
 * @param table  目标表对象
 * @param tuples 要插入的元组列表（支持多行插入）
 */
InsertStmt::InsertStmt(Table *table, const std::vector<InsertTuple> tuples) : table_(table), tuples_(tuples) {}

/**
 * @brief 创建InsertStmt对象（INSERT语句的语义解析核心方法）
 * 
 * 【insert和null功能的核心入口 - Resolver阶段】
 * 
 * @param db      数据库实例
 * @param inserts Parser阶段生成的INSERT语法树节点
 * @param stmt    输出参数，创建成功后指向InsertStmt对象
 * @return RC     成功返回SUCCESS，失败返回对应错误码
 * 
 * 校验点：
 * 1. RC::SCHEMA_TABLE_NOT_EXIST    - 表不存在
 * 2. RC::SCHEMA_FIELD_MISSING      - 字段数量不匹配
 * 3. RC::NULL_CANT_INSERT          - 向NOT NULL字段插入NULL
 * 4. RC::SCHEMA_FIELD_TYPE_MISMATCH - 类型不匹配且无法转换
 */
RC InsertStmt::create(Db *db, const InsertSqlNode &inserts, Stmt *&stmt)
{
  // ======================== 第1步：基本参数验证 ========================
  const char *table_name = inserts.relation_name.c_str();
  if (nullptr == db || nullptr == table_name || inserts.tuples.empty()) {
    LOG_WARN("invalid argument. db=%p, table_name=%p, value_num=%d",
        db, table_name, static_cast<int>(inserts.tuples.size()));
    return RC::INVALID_ARGUMENT;
  }

  // ======================== 第2步：表存在性检查 ========================
  // 检查目标表是否存在
  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }
  
  // ======================== 第3步：遍历每行数据进行校验 ========================
  // 【insert多行功能】支持一次INSERT插入多行数据
  // 语法：INSERT INTO t VALUES (1,2), (3,4), (5,6);
  for (auto &values : inserts.tuples) {
    const int        value_num  = static_cast<int>(values.size());
    const TableMeta &table_meta = table->table_meta();
    // 计算用户可见字段数（总字段数 - 系统字段数）
    const int        field_num  = table_meta.field_num() - table_meta.sys_field_num();

    // 字段数量检查：插入值数量必须等于字段数量
    if (field_num != value_num) {
      LOG_WARN("schema mismatch. value num=%d, field num in schema=%d", value_num, field_num);
      return RC::SCHEMA_FIELD_MISSING;
    }
    
    // ======================== 第4步：类型检查和NULL检查 ========================
    const int sys_filed_num = table_meta.sys_field_num();
    for (int i = 0; i < field_num; i++) {
      // 获取字段元数据
      const FieldMeta *field_meta = table_meta.field(i + sys_filed_num);
      const AttrType   field_type = field_meta->type();
      const AttrType   value_type = values[i].attr_type();
      
      // 类型不匹配时需要特殊处理
      if (value_type != field_type) {
        Value real_value;
        
        // 【null功能核心】检查NULL值
        if (values[i].is_null())  // 插入的是一个NULL值
        {
          // 检查该字段是否允许NULL
          if (not field_meta->nullable()) {  // 该列定义为 NOT NULL
            // 【null功能关键返回点】不允许向NOT NULL字段插入NULL
            return RC::NULL_CANT_INSERT;
          }
          // 允许NULL，后续在make_record中会处理
        } else {
          // 非NULL值，尝试类型转换
          // 例如：字符串"2024-01-01"转换为DATE类型
          RC rc = Value::cast_to(values[i], field_meta->type(), real_value);
          if (OB_FAIL(rc)) {
            // 类型转换失败
            LOG_WARN("failed to cast value. table name:%s,field name:%s,value:%s ",
              table_meta.name(), field_meta->name(), values[i].to_string().c_str());
            return rc;
          }
        }
      }
    }
  }
  
  // 调试日志：打印所有要插入的数据
  for (auto raws : inserts.tuples) {
    for (auto e : raws) {
      LOG_WARN("here %d",*e.data());
    }
  }
  
  // ======================== 第5步：创建InsertStmt对象 ========================
  // 所有校验通过，创建InsertStmt
  stmt = new InsertStmt(table, inserts.tuples);
  return RC::SUCCESS;
}
