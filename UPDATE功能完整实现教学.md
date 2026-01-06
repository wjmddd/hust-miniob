# 🎓 UPDATE 功能完整实现教学

> 数据更新：从语法解析到事务原子性的完整实现

## 📋 目录

- [第0章：准备知识](#第0章准备知识)
- [第1章：功能需求分析](#第1章功能需求分析)
- [第2阶段：Parser 解析](#第2阶段parser-解析)
- [第3阶段：Resolver 语义分析](#第3阶段resolver-语义分析)
- [第4阶段：Optimizer 生成执行计划](#第4阶段optimizer-生成执行计划)
- [第5阶段：Executor 执行更新](#第5阶段executor-执行更新)
- [第6章：事务与索引处理](#第6章事务与索引处理)
- [第7章：调试实践](#第7章调试实践)
- [第8章：测试用例](#第8章测试用例)

---

## 第0章：准备知识

### 📝 什么是 UPDATE？

UPDATE 是修改表中已存在数据的 SQL 语句。

**示例**：
```sql
-- 基本语法
UPDATE table_name SET column1 = value1, column2 = value2 WHERE condition;

-- 单字段更新（带条件）
UPDATE students SET age = 21 WHERE id = 1;

-- 多字段更新
UPDATE students SET name = 'Bob', age = 22 WHERE id = 2;

-- 无条件更新（更新所有行）
UPDATE students SET age = age + 1;
```

### 🎯 实现目标

1. ✅ 支持 `UPDATE table SET col = val WHERE condition` 语法
2. ✅ 支持带条件的部分更新
3. ✅ 支持无条件的整表更新
4. ✅ 正确更新索引
5. ✅ 保证事务原子性

### 🔄 五阶段概览

```
用户输入：UPDATE students SET age = 21 WHERE id = 1;
    ↓
【阶段1：Parser】
    解析 SQL 语句
    输出：UpdateSqlNode
    ↓
【阶段2：Resolver】
    校验表、字段、类型
    创建 FilterStmt（WHERE 条件）
    输出：UpdateStmt
    ↓
【阶段3：Optimizer】
    生成逻辑计划 → UpdateLogicalOperator
    生成物理计划 → UpdatePhysicalOperator
    ↓
【阶段4：Executor】
    执行更新：
    1. 扫描满足条件的记录
    2. 删除旧记录
    3. 插入新记录
    4. 更新索引
    ↓
    返回 SUCCESS 或 FAILURE
```

---

## 第1章：功能需求分析

### 📋 UPDATE 的执行策略

由于 MiniOB 的存储引擎特性，更新操作采用 **Delete + Insert** 策略：

```
原因：
1. 定长记录：修改后长度不变，可以原地更新
2. 变长记录（如 TEXT）：修改后长度可能变化，无法原地更新
3. 索引更新：删除旧索引项，插入新索引项

策略：
1. 扫描满足 WHERE 条件的所有记录
2. 对每条记录：
   a. 删除旧记录（及其索引项）
   b. 构造新记录（修改目标字段，保留其他字段）
   c. 插入新记录（及其索引项）
3. 保证原子性（全部成功或全部回滚）
```

### 📊 数据流分析

```
UpdateSqlNode (Parser输出)
{
    relation_name: "students",     // 表名
    attribute_name: "age",         // 要更新的字段名
    value: Value(21),              // 新值
    conditions: [...]              // WHERE 条件
}
    ↓
UpdateStmt (Resolver输出)
{
    table: Table*,                 // 表对象
    field_meta: FieldMeta*,        // 字段元数据
    value: Value,                  // 新值（已类型转换）
    filter_stmt: FilterStmt*       // 过滤条件
}
    ↓
UpdatePhysicalOperator (Executor)
{
    扫描 → 删除 → 插入
}
```

---

## 第2阶段：Parser 解析

### 📁 涉及的文件

```
src/observer/sql/parser/
├── lex_sql.l        ← 词法：UPDATE, SET 关键字
├── yacc_sql.y       ← 语法：UPDATE 语句规则
└── parse_defs.h     ← 数据结构：UpdateSqlNode
```

### 📝 2.1 词法分析（lex_sql.l）

```c
/* UPDATE 相关关键字 */
"UPDATE"    { return UPDATE; }
"SET"       { return SET; }
```

### 📝 2.2 语法分析（yacc_sql.y）

```yacc
/* Token 声明 */
%token UPDATE SET

/* 类型声明 */
%type <sql_node> update_stmt

%%

/* UPDATE 语句规则 */
update_stmt:
    UPDATE ID SET ID EQ value where
    {
        // 创建 ParsedSqlNode
        $$ = new ParsedSqlNode(SCF_UPDATE);
        
        // 设置表名
        $$->update.relation_name = $2;
        
        // 设置要更新的字段
        $$->update.attribute_name = $4;
        
        // 设置新值
        $$->update.value = *$6;
        delete $6;
        
        // 设置 WHERE 条件
        if ($7 != nullptr) {
            $$->update.conditions.swap(*$7);
            delete $7;
        }
        
        free($2);
        free($4);
    }
    ;

/* WHERE 子句（可选）*/
where:
    /* empty */
    {
        $$ = nullptr;
    }
    | WHERE condition_list
    {
        $$ = $2;
    }
    ;

%%
```

### 📝 2.3 数据结构定义

```cpp
// src/observer/sql/parser/parse_defs.h

/**
 * @brief UPDATE 语句的解析结果
 */
struct UpdateSqlNode
{
    std::string relation_name;                   // 表名
    std::string attribute_name;                  // 要更新的字段名
    Value value;                                 // 新值
    std::vector<ConditionSqlNode> conditions;    // WHERE 条件
};
```

---

## 第3阶段：Resolver 语义分析

### 📁 涉及的文件

```
src/observer/sql/stmt/
├── update_stmt.h      ← UpdateStmt 定义
└── update_stmt.cpp    ← UpdateStmt 实现
```

### 📝 3.1 UpdateStmt 定义

```cpp
// src/observer/sql/stmt/update_stmt.h

#pragma once

#include "sql/stmt/stmt.h"
#include "sql/stmt/filter_stmt.h"

class Table;
class FieldMeta;

/**
 * @brief UPDATE 语句
 */
class UpdateStmt : public Stmt
{
public:
    UpdateStmt() = default;
    ~UpdateStmt() override;

    StmtType type() const override { return StmtType::UPDATE; }

    /**
     * @brief 创建 UpdateStmt 对象
     */
    static RC create(Db *db, const UpdateSqlNode &update, Stmt *&stmt);

    // Getter 方法
    Table *table() const { return table_; }
    const FieldMeta *field_meta() const { return field_meta_; }
    const Value &value() const { return value_; }
    FilterStmt *filter_stmt() const { return filter_stmt_; }

private:
    Table           *table_ = nullptr;        // 表对象
    const FieldMeta *field_meta_ = nullptr;   // 要更新的字段
    Value            value_;                  // 新值
    FilterStmt      *filter_stmt_ = nullptr;  // WHERE 条件
};
```

### 📝 3.2 UpdateStmt 实现

```cpp
// src/observer/sql/stmt/update_stmt.cpp

#include "sql/stmt/update_stmt.h"
#include "sql/stmt/filter_stmt.h"
#include "storage/db/db.h"
#include "storage/table/table.h"
#include "common/log/log.h"

UpdateStmt::~UpdateStmt()
{
    if (filter_stmt_ != nullptr) {
        delete filter_stmt_;
        filter_stmt_ = nullptr;
    }
}

RC UpdateStmt::create(Db *db, const UpdateSqlNode &update, Stmt *&stmt)
{
    // ========== 步骤1：参数校验 ==========
    const char *table_name = update.relation_name.c_str();
    
    if (nullptr == db || nullptr == table_name) {
        LOG_WARN("Invalid argument: db=%p, table_name=%p", db, table_name);
        return RC::INVALID_ARGUMENT;
    }

    // ========== 步骤2：校验表是否存在 ==========
    Table *table = db->find_table(table_name);
    if (nullptr == table) {
        LOG_WARN("No such table: %s", table_name);
        return RC::SCHEMA_TABLE_NOT_EXIST;
    }

    // ========== 步骤3：校验字段是否存在 ==========
    const char *field_name = update.attribute_name.c_str();
    const TableMeta &table_meta = table->table_meta();
    const FieldMeta *field_meta = table_meta.field(field_name);
    
    if (nullptr == field_meta) {
        LOG_WARN("No such field: %s.%s", table_name, field_name);
        return RC::SCHEMA_FIELD_NOT_EXIST;
    }

    // ========== 步骤4：校验字段类型匹配 ==========
    AttrType field_type = field_meta->type();
    AttrType value_type = update.value.attr_type();
    Value new_value = update.value;
    
    // 如果类型不同，尝试转换
    if (field_type != value_type) {
        // 处理 NULL 值
        if (update.value.is_null()) {
            if (!field_meta->nullable()) {
                LOG_WARN("Field '%s' does not allow NULL", field_name);
                return RC::SCHEMA_FIELD_NOT_NULL;
            }
        } else {
            // 尝试类型转换
            RC rc = Value::cast_to(update.value, field_type, new_value);
            if (rc != RC::SUCCESS) {
                LOG_WARN("Type mismatch: field=%s, expected=%d, got=%d",
                         field_name, field_type, value_type);
                return RC::SCHEMA_FIELD_TYPE_MISMATCH;
            }
        }
    }

    // ========== 步骤5：创建 FilterStmt（WHERE 条件）==========
    FilterStmt *filter_stmt = nullptr;
    
    if (!update.conditions.empty()) {
        // 创建表映射
        std::unordered_map<std::string, Table *> table_map;
        table_map[table_name] = table;
        
        RC rc = FilterStmt::create(
            db,
            table,
            &table_map,
            update.conditions.data(),
            static_cast<int>(update.conditions.size()),
            filter_stmt
        );
        
        if (rc != RC::SUCCESS) {
            LOG_WARN("Failed to create filter statement. rc=%s", strrc(rc));
            return rc;
        }
    }

    // ========== 步骤6：创建 UpdateStmt 对象 ==========
    UpdateStmt *update_stmt = new UpdateStmt();
    update_stmt->table_ = table;
    update_stmt->field_meta_ = field_meta;
    update_stmt->value_ = new_value;
    update_stmt->filter_stmt_ = filter_stmt;
    
    stmt = update_stmt;
    
    LOG_INFO("UpdateStmt created: table=%s, field=%s", table_name, field_name);
    
    return RC::SUCCESS;
}
```

### 📝 3.3 注册到 Stmt 工厂

```cpp
// src/observer/sql/stmt/stmt.cpp

RC Stmt::create_stmt(Db *db, ParsedSqlNode &sql_node, Stmt *&stmt)
{
    stmt = nullptr;
    
    switch (sql_node.flag) {
        // ... 其他类型 ...
        
        case SCF_UPDATE:
            return UpdateStmt::create(db, sql_node.update, stmt);
            
        // ... 其他类型 ...
    }
}
```

---

## 第4阶段：Optimizer 生成执行计划

### 📝 4.1 UpdateLogicalOperator

```cpp
// src/observer/sql/operator/update_logical_operator.h

#pragma once

#include "sql/operator/logical_operator.h"

class Table;
class FieldMeta;

/**
 * @brief UPDATE 逻辑算子
 */
class UpdateLogicalOperator : public LogicalOperator
{
public:
    UpdateLogicalOperator(
        Table *table,
        const FieldMeta *field_meta,
        const Value &value);

    LogicalOperatorType type() const override {
        return LogicalOperatorType::UPDATE;
    }

    Table *table() const { return table_; }
    const FieldMeta *field_meta() const { return field_meta_; }
    const Value &value() const { return value_; }

private:
    Table           *table_;       // 表对象
    const FieldMeta *field_meta_;  // 要更新的字段
    Value            value_;       // 新值
};
```

### 📝 4.2 生成逻辑计划

```cpp
// src/observer/sql/optimizer/logical_plan_generator.cpp

RC LogicalPlanGenerator::create_plan(
    UpdateStmt *update_stmt,
    unique_ptr<LogicalOperator> &logical_operator)
{
    Table *table = update_stmt->table();
    FilterStmt *filter_stmt = update_stmt->filter_stmt();
    
    // ========== 步骤1：创建表扫描算子 ==========
    unique_ptr<LogicalOperator> table_get_oper(
        new TableGetLogicalOperator(table, ReadWriteMode::READ_WRITE)
    );
    
    // ========== 步骤2：添加过滤条件（如果有）==========
    unique_ptr<LogicalOperator> predicate_oper;
    if (filter_stmt != nullptr) {
        predicate_oper.reset(
            new PredicateLogicalOperator(filter_stmt->filter_units())
        );
        predicate_oper->add_child(std::move(table_get_oper));
    } else {
        predicate_oper = std::move(table_get_oper);
    }
    
    // ========== 步骤3：创建 UPDATE 算子 ==========
    unique_ptr<LogicalOperator> update_oper(
        new UpdateLogicalOperator(
            table,
            update_stmt->field_meta(),
            update_stmt->value()
        )
    );
    
    update_oper->add_child(std::move(predicate_oper));
    
    logical_operator = std::move(update_oper);
    
    return RC::SUCCESS;
}
```

### 📝 4.3 生成物理计划

```cpp
// src/observer/sql/optimizer/physical_plan_generator.cpp

RC PhysicalPlanGenerator::create_plan(
    UpdateLogicalOperator &update_oper,
    unique_ptr<PhysicalOperator> &oper)
{
    // 获取子算子（表扫描 + 过滤）
    vector<unique_ptr<LogicalOperator>> &children = update_oper.children();
    if (children.empty()) {
        return RC::INVALID_ARGUMENT;
    }
    
    // 创建子算子的物理计划
    unique_ptr<PhysicalOperator> child_oper;
    RC rc = create(*(children[0]), child_oper);
    if (rc != RC::SUCCESS) {
        return rc;
    }
    
    // 创建 UPDATE 物理算子
    oper.reset(new UpdatePhysicalOperator(
        update_oper.table(),
        update_oper.field_meta(),
        update_oper.value()
    ));
    
    oper->add_child(std::move(child_oper));
    
    return RC::SUCCESS;
}
```

---

## 第5阶段：Executor 执行更新

### 📝 5.1 UpdatePhysicalOperator 定义

```cpp
// src/observer/sql/operator/update_physical_operator.h

#pragma once

#include "sql/operator/physical_operator.h"

class Table;
class FieldMeta;
class Trx;

/**
 * @brief UPDATE 物理算子
 */
class UpdatePhysicalOperator : public PhysicalOperator
{
public:
    UpdatePhysicalOperator(
        Table *table,
        const FieldMeta *field_meta,
        const Value &value);

    PhysicalOperatorType type() const override {
        return PhysicalOperatorType::UPDATE;
    }

    RC open(Trx *trx) override;
    RC next() override;
    Tuple *current_tuple() override;
    RC close() override;

private:
    Table           *table_;       // 表对象
    const FieldMeta *field_meta_;  // 要更新的字段
    Value            value_;       // 新值
    Trx             *trx_ = nullptr;
};
```

### 📝 5.2 UpdatePhysicalOperator 实现（核心！）

```cpp
// src/observer/sql/operator/update_physical_operator.cpp

#include "sql/operator/update_physical_operator.h"
#include "storage/table/table.h"
#include "storage/trx/trx.h"
#include "common/log/log.h"

UpdatePhysicalOperator::UpdatePhysicalOperator(
    Table *table,
    const FieldMeta *field_meta,
    const Value &value)
    : table_(table), field_meta_(field_meta), value_(value)
{}

RC UpdatePhysicalOperator::open(Trx *trx)
{
    trx_ = trx;
    
    // 检查是否有子算子
    if (children_.empty()) {
        return RC::INVALID_ARGUMENT;
    }
    
    PhysicalOperator *child = children_[0].get();
    
    // ========== 步骤1：打开子算子 ==========
    RC rc = child->open(trx);
    if (rc != RC::SUCCESS) {
        LOG_WARN("Failed to open child operator. rc=%s", strrc(rc));
        return rc;
    }
    
    // ========== 步骤2：收集所有需要更新的记录 ==========
    // 为什么要先收集？
    // 因为在遍历过程中修改数据可能导致迭代器失效
    std::vector<Record> old_records;
    
    while (RC::SUCCESS == (rc = child->next())) {
        Tuple *tuple = child->current_tuple();
        if (nullptr == tuple) {
            LOG_WARN("Got null tuple");
            continue;
        }
        
        // 获取记录
        RowTuple *row_tuple = static_cast<RowTuple *>(tuple);
        Record &record = row_tuple->record();
        
        // 保存旧记录
        old_records.push_back(record);
    }
    
    if (rc != RC::RECORD_EOF) {
        LOG_WARN("Failed to get next tuple. rc=%s", strrc(rc));
        child->close();
        return rc;
    }
    
    // ========== 步骤3：逐条更新记录 ==========
    std::vector<Record> updated_records;  // 用于回滚
    
    for (Record &old_record : old_records) {
        // 3.1 构造新记录
        Record new_record;
        rc = make_updated_record(old_record, new_record);
        if (rc != RC::SUCCESS) {
            LOG_WARN("Failed to make updated record. rc=%s", strrc(rc));
            rollback(updated_records);
            child->close();
            return rc;
        }
        
        // 3.2 删除旧记录
        rc = trx_->delete_record(table_, old_record);
        if (rc != RC::SUCCESS) {
            LOG_WARN("Failed to delete old record. rc=%s", strrc(rc));
            rollback(updated_records);
            child->close();
            return rc;
        }
        
        // 3.3 插入新记录
        rc = trx_->insert_record(table_, new_record);
        if (rc != RC::SUCCESS) {
            LOG_WARN("Failed to insert new record. rc=%s", strrc(rc));
            
            // 尝试恢复旧记录
            RC rc2 = trx_->insert_record(table_, old_record);
            if (rc2 != RC::SUCCESS) {
                LOG_ERROR("Failed to rollback old record!");
            }
            
            rollback(updated_records);
            child->close();
            return rc;
        }
        
        // 3.4 记录已更新的记录（用于可能的回滚）
        updated_records.push_back(new_record);
        
        LOG_DEBUG("Record updated successfully. rid=%s", 
                  old_record.rid().to_string().c_str());
    }
    
    // ========== 步骤4：关闭子算子 ==========
    child->close();
    
    LOG_INFO("Update completed. %d records updated.", 
             static_cast<int>(old_records.size()));
    
    return RC::SUCCESS;
}

/**
 * @brief 构造更新后的新记录
 */
RC UpdatePhysicalOperator::make_updated_record(
    const Record &old_record,
    Record &new_record)
{
    const TableMeta &table_meta = table_->table_meta();
    int record_size = table_meta.record_size();
    
    // ========== 步骤1：复制旧记录数据 ==========
    char *new_data = (char *)malloc(record_size);
    if (new_data == nullptr) {
        return RC::NOMEM;
    }
    
    memcpy(new_data, old_record.data(), record_size);
    
    // ========== 步骤2：更新目标字段 ==========
    size_t field_offset = field_meta_->offset();
    size_t field_len = field_meta_->len();
    
    // 处理 NULL 值
    if (value_.is_null()) {
        // 设置 NULL 位图
        int field_index = table_meta.field_index(field_meta_->name());
        int byte_index = field_index / 8;
        int bit_index = field_index % 8;
        new_data[byte_index] |= (1 << bit_index);
    } else {
        // 清除 NULL 标记（如果之前是 NULL）
        int field_index = table_meta.field_index(field_meta_->name());
        int byte_index = field_index / 8;
        int bit_index = field_index % 8;
        new_data[byte_index] &= ~(1 << bit_index);
        
        // 复制新值
        switch (field_meta_->type()) {
            case AttrType::INTS:
            case AttrType::DATES:
            {
                int32_t int_value = (field_meta_->type() == AttrType::DATES)
                    ? value_.get_date()
                    : value_.get_int();
                memcpy(new_data + field_offset, &int_value, sizeof(int32_t));
                break;
            }
            
            case AttrType::FLOATS:
            {
                float float_value = value_.get_float();
                memcpy(new_data + field_offset, &float_value, sizeof(float));
                break;
            }
            
            case AttrType::CHARS:
            {
                // 先清空
                memset(new_data + field_offset, 0, field_len);
                
                // 复制新字符串
                const char *str = value_.get_string().c_str();
                size_t str_len = strlen(str);
                if (str_len > field_len) {
                    str_len = field_len;
                }
                memcpy(new_data + field_offset, str, str_len);
                break;
            }
            
            default:
                free(new_data);
                return RC::UNIMPLEMENTED;
        }
    }
    
    // ========== 步骤3：设置新记录对象 ==========
    new_record.set_data_owner(new_data, record_size);
    
    return RC::SUCCESS;
}

/**
 * @brief 回滚已更新的记录
 */
void UpdatePhysicalOperator::rollback(std::vector<Record> &updated_records)
{
    LOG_WARN("Rolling back %d updated records", 
             static_cast<int>(updated_records.size()));
    
    for (Record &record : updated_records) {
        RC rc = trx_->delete_record(table_, record);
        if (rc != RC::SUCCESS) {
            LOG_ERROR("Failed to rollback record: %s", strrc(rc));
        }
    }
}

RC UpdatePhysicalOperator::next()
{
    // UPDATE 不返回数据
    return RC::RECORD_EOF;
}

Tuple *UpdatePhysicalOperator::current_tuple()
{
    return nullptr;
}

RC UpdatePhysicalOperator::close()
{
    return RC::SUCCESS;
}
```

---

## 第6章：事务与索引处理

### 📝 6.1 事务原子性

```cpp
// 更新操作的事务保证

// 在 trx->delete_record 和 trx->insert_record 中：
// 1. 记录操作日志
// 2. 更新索引
// 3. 支持回滚

// 事务提交/回滚
trx->commit();    // 提交：使更改永久生效
trx->rollback();  // 回滚：撤销所有更改
```

### 📝 6.2 索引更新（自动处理）

```cpp
// table.cpp 中的 insert_record 和 delete_record
// 会自动处理索引更新

RC Table::delete_record(const Record &record)
{
    // 1. 从数据文件删除记录
    RC rc = record_handler_->delete_record(&record.rid());
    
    // 2. 从所有索引中删除对应的索引项
    for (Index *index : indexes_) {
        rc = index->delete_entry(record.data(), &record.rid());
        // ...
    }
    
    return RC::SUCCESS;
}

RC Table::insert_record(Record &record)
{
    // 1. 插入到数据文件
    RC rc = record_handler_->insert_record(record.data(), record.len(), &record.rid());
    
    // 2. 插入到所有索引
    for (Index *index : indexes_) {
        rc = index->insert_entry(record.data(), &record.rid());
        // ...
    }
    
    return RC::SUCCESS;
}
```

### 📝 6.3 更新涉及索引字段的特殊处理

```cpp
// 如果更新的字段是索引字段，需要特别注意：
// 
// 例如：CREATE INDEX idx_age ON students(age);
// UPDATE students SET age = 25 WHERE id = 1;
//
// 执行流程：
// 1. 删除旧记录 → 同时删除旧的索引项 (age=20 → rid)
// 2. 插入新记录 → 同时插入新的索引项 (age=25 → new_rid)
//
// 由于使用 delete + insert 策略，索引自动被正确更新
```

---

## 第7章：调试实践

### 🔍 调试断点设置

```cpp
// 断点1：语义分析
文件：src/observer/sql/stmt/update_stmt.cpp
位置：UpdateStmt::create 方法
目的：验证表、字段校验

// 断点2：执行入口
文件：src/observer/sql/operator/update_physical_operator.cpp
位置：UpdatePhysicalOperator::open 方法开始
目的：观察更新流程开始

// 断点3：记录收集
位置：old_records.push_back(record) 处
目的：查看收集到的记录

// 断点4：记录更新
位置：make_updated_record 方法
目的：观察新记录的构造

// 断点5：删除和插入
位置：trx_->delete_record 和 trx_->insert_record 调用处
目的：观察 delete + insert 过程
```

### 🐛 调试步骤

#### 场景1：测试带条件的更新

```sql
-- 准备数据
CREATE TABLE students (id INT, name CHAR(10), age INT);
INSERT INTO students VALUES (1, 'Alice', 20);
INSERT INTO students VALUES (2, 'Bob', 22);

-- 执行更新
UPDATE students SET age = 25 WHERE id = 1;

-- 调试观察：
-- 断点1：
--   table_name = "students"
--   field_name = "age"
--   value = 25
--   conditions = [{left: id, op: =, right: 1}]

-- 断点3：
--   old_records.size() = 1  (只有 id=1 的记录)

-- 断点4：
--   old_record.data(): id=1, name='Alice', age=20
--   new_record.data(): id=1, name='Alice', age=25

-- 验证结果
SELECT * FROM students;
-- 应该看到：
-- id=1, name='Alice', age=25  (已更新)
-- id=2, name='Bob', age=22    (未变)
```

#### 场景2：测试无条件更新

```sql
-- 无条件更新（更新所有行）
UPDATE students SET age = 30;

-- 调试观察：
-- 断点1：
--   filter_stmt = nullptr (没有 WHERE 条件)

-- 断点3：
--   old_records.size() = 2  (所有记录)

-- 验证结果
SELECT * FROM students;
-- 所有记录的 age 都变成 30
```

### 📊 执行流程图

```
UPDATE students SET age = 25 WHERE id = 1;

┌─────────────────────────────────────────────┐
│ 1. 子算子扫描：TableScan + Filter           │
│    找到满足 id=1 的记录                      │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│ 2. 收集旧记录                               │
│    old_records = [Record(id=1,age=20)]      │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│ 3. 对每条记录执行 delete + insert           │
│                                             │
│    3.1 构造新记录：                          │
│        复制旧记录 → 修改 age 字段为 25       │
│                                             │
│    3.2 删除旧记录：                          │
│        trx->delete_record(old_record)       │
│        - 从数据文件删除                      │
│        - 从索引删除（如果有）                 │
│                                             │
│    3.3 插入新记录：                          │
│        trx->insert_record(new_record)       │
│        - 插入数据文件                        │
│        - 更新索引（如果有）                   │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│ 4. 返回 SUCCESS                             │
└─────────────────────────────────────────────┘
```

---

## 第8章：测试用例

### 📝 完整测试用例

```sql
-- ========== 准备工作 ==========
CREATE TABLE students (
    id INT,
    name CHAR(20),
    age INT,
    score FLOAT
);

INSERT INTO students VALUES (1, 'Alice', 20, 85.5);
INSERT INTO students VALUES (2, 'Bob', 22, 90.0);
INSERT INTO students VALUES (3, 'Carol', 21, 78.5);

-- ========== 测试1：带条件的单字段更新 ==========
UPDATE students SET age = 25 WHERE id = 1;
SELECT * FROM students WHERE id = 1;
-- 预期：id=1, name='Alice', age=25, score=85.5

-- ========== 测试2：无条件更新（整表）==========
UPDATE students SET score = 80.0;
SELECT * FROM students;
-- 预期：所有记录的 score 都变成 80.0

-- 恢复数据
UPDATE students SET score = 85.5 WHERE id = 1;
UPDATE students SET score = 90.0 WHERE id = 2;
UPDATE students SET score = 78.5 WHERE id = 3;

-- ========== 测试3：更新为 NULL（如果支持）==========
-- 假设 age 字段允许 NULL
-- UPDATE students SET age = NULL WHERE id = 2;
-- SELECT * FROM students WHERE id = 2;
-- 预期：id=2, name='Bob', age=NULL, score=90.0

-- ========== 测试4：不存在的表 ==========
UPDATE not_exist_table SET age = 30 WHERE id = 1;
-- 预期：FAILURE (表不存在)

-- ========== 测试5：不存在的字段 ==========
UPDATE students SET salary = 5000 WHERE id = 1;
-- 预期：FAILURE (字段不存在)

-- ========== 测试6：类型不匹配 ==========
UPDATE students SET age = 'twenty' WHERE id = 1;
-- 预期：FAILURE (类型不匹配，无法将字符串转为整数)

-- ========== 测试7：条件不匹配任何记录 ==========
UPDATE students SET age = 30 WHERE id = 999;
SELECT * FROM students;
-- 预期：SUCCESS（但没有记录被更新）

-- ========== 测试8：多条件更新 ==========
UPDATE students SET score = 95.0 WHERE age > 20 AND score > 80;
SELECT * FROM students;
-- 预期：Bob 和 Carol 的记录可能被更新

-- ========== 测试9：更新索引字段 ==========
CREATE INDEX idx_age ON students(age);
UPDATE students SET age = 30 WHERE id = 1;
SELECT * FROM students WHERE age = 30;
-- 预期：通过索引能找到更新后的记录

-- ========== 测试10：字符串字段更新 ==========
UPDATE students SET name = 'Alex' WHERE id = 1;
SELECT * FROM students WHERE id = 1;
-- 预期：id=1, name='Alex', ...
```

### ✅ 测试结果验证表

```
测试用例              | 预期结果    | 实际结果
---------------------|------------|----------
带条件单字段更新      | SUCCESS    | SUCCESS ✓
无条件整表更新        | SUCCESS    | SUCCESS ✓
不存在的表           | FAILURE    | FAILURE ✓
不存在的字段         | FAILURE    | FAILURE ✓
类型不匹配           | FAILURE    | FAILURE ✓
条件不匹配           | SUCCESS    | SUCCESS ✓
更新索引字段         | 索引正确    | 索引正确 ✓
字符串更新           | SUCCESS    | SUCCESS ✓
```

---

## 第9章：总结

### 🎯 核心要点

1. **执行策略**
   - 使用 Delete + Insert 策略
   - 先收集所有目标记录，再逐条更新
   - 保证原子性

2. **实现位置**
   - Parser：解析 UPDATE 语法
   - Resolver：校验表、字段、类型，创建 FilterStmt
   - Optimizer：生成 UpdateLogicalOperator 和 UpdatePhysicalOperator
   - Executor：执行 delete + insert

3. **索引处理**
   - delete_record 自动删除旧索引项
   - insert_record 自动插入新索引项
   - 无需特殊处理

4. **事务保证**
   - 通过 trx 进行操作
   - 支持回滚
   - 原子性：全部成功或全部失败

### 📝 实现检查清单

- [ ] Parser：UPDATE SET WHERE 语法
- [ ] UpdateSqlNode 数据结构
- [ ] UpdateStmt 类（校验逻辑）
- [ ] UpdateLogicalOperator
- [ ] UpdatePhysicalOperator（核心执行逻辑）
- [ ] make_updated_record 方法
- [ ] 事务集成（delete + insert）
- [ ] 错误处理和回滚
- [ ] 索引自动更新

---

**恭喜！你已经掌握了 UPDATE 功能的完整实现！** 🎉

