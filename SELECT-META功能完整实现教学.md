# 🎓 SELECT-META 功能完整实现教学

> 元数据校验：确保查询中的表名、字段名合法

## 📋 目录

- [第0章：准备知识](#第0章准备知识)
- [第1章：功能需求分析](#第1章功能需求分析)
- [第2章：实现位置分析](#第2章实现位置分析)
- [第3章：核心实现详解](#第3章核心实现详解)
- [第4章：完整代码实现](#第4章完整代码实现)
- [第5章：调试实践](#第5章调试实践)
- [第6章：测试用例](#第6章测试用例)
- [第7章：常见问题与解决](#第7章常见问题与解决)

---

## 第0章：准备知识

### 📝 什么是 SELECT-META？

SELECT-META（元数据校验）是指在执行 SQL 查询时，系统需要验证：
1. **表是否存在**：`SELECT * FROM not_exist_table;` 应报错
2. **字段是否存在**：`SELECT not_exist_col FROM t;` 应报错
3. **字段是否属于指定表**：多表查询时，`t1.col` 中的 `col` 是否真的属于 `t1`

### 🎯 为什么需要元数据校验？

```sql
-- 场景1：表不存在
SELECT * FROM students;  -- 如果 students 表不存在，应该报错

-- 场景2：字段不存在
SELECT name, age, salary FROM employees;  -- 如果 salary 字段不存在，应该报错

-- 场景3：多表查询时字段归属错误
SELECT t1.id, t2.name FROM t1, t2 WHERE t1.age > 18;
-- 如果 age 不属于 t1，应该报错
```

### 🔄 五阶段中的位置

```
用户输入：SELECT * FROM not_exist_table;
    ↓
【阶段1：Parser】
    解析 SQL 字符串 → 语法树
    ✓ 语法正确，Parser 通过
    ↓
【阶段2：Resolver】 ← 🎯 元数据校验在这里！
    验证表、字段是否存在
    ✗ 表不存在，返回错误
    ↓
【不会执行到后续阶段】
```

**关键点**：元数据校验主要在 **Resolver（语义分析）** 阶段完成。

---

## 第1章：功能需求分析

### 📋 需要校验的场景

#### 1.1 SELECT 语句中的校验点

```sql
SELECT 
    col1, col2, t1.col3    -- 校验点1：SELECT 列表中的字段
FROM 
    table1, table2         -- 校验点2：FROM 子句中的表
WHERE 
    col4 > 10              -- 校验点3：WHERE 条件中的字段
    AND t1.col5 = 'abc'    -- 校验点4：带表名前缀的字段
ORDER BY 
    col6                   -- 校验点5：ORDER BY 中的字段
GROUP BY 
    col7;                  -- 校验点6：GROUP BY 中的字段
```

#### 1.2 错误码定义

```cpp
// src/observer/common/rc.h
enum class RC {
    SUCCESS = 0,
    // ...
    SCHEMA_TABLE_NOT_EXIST,      // 表不存在
    SCHEMA_FIELD_NOT_EXIST,      // 字段不存在
    SCHEMA_FIELD_MISSING,        // 字段数量不匹配
    SCHEMA_FIELD_TYPE_MISMATCH,  // 字段类型不匹配
    // ...
};
```

### 📊 数据流分析

```
ParsedSqlNode (Parser输出)
{
    flag: SCF_SELECT,
    selection: {
        relations: ["students"],           // 表名（字符串）
        attributes: [                      // 字段（字符串）
            {relation: "", attribute: "*"},
            {relation: "students", attribute: "name"}
        ],
        conditions: [...]                  // WHERE 条件
    }
}
    ↓
【Resolver 校验】
    1. relations 中的表名是否存在？
    2. attributes 中的字段名是否存在？
    3. conditions 中的字段名是否存在？
    ↓
SelectStmt (Resolver输出)
{
    tables: [Table*],        // 表对象指针（不是字符串了）
    query_fields: [Field*],  // 字段对象指针
    filter_stmt: FilterStmt* // 过滤条件
}
```

---

## 第2章：实现位置分析

### 📁 涉及的文件

```
src/observer/sql/stmt/
├── stmt.h                  ← Stmt 基类
├── stmt.cpp                ← Stmt 工厂方法
├── select_stmt.h           ← SelectStmt 定义
└── select_stmt.cpp         ← 🎯 核心实现文件

src/observer/storage/table/
├── table.h                 ← Table 类定义
└── table.cpp               ← 表操作

src/observer/storage/field/
├── field_meta.h            ← FieldMeta 定义
└── field_meta.cpp          ← 字段元数据

src/observer/storage/db/
├── db.h                    ← Db 类定义
└── db.cpp                  ← 数据库操作（find_table）
```

### 🔍 关键类和方法

#### 2.1 Db 类：查找表

```cpp
// src/observer/storage/db/db.h
class Db {
public:
    /**
     * @brief 根据表名查找表对象
     * @param table_name 表名
     * @return Table* 表对象指针，不存在返回 nullptr
     */
    Table *find_table(const char *table_name) const;
    
    // ...
};
```

#### 2.2 Table 类：获取表元数据

```cpp
// src/observer/storage/table/table.h
class Table {
public:
    /**
     * @brief 获取表的元数据
     */
    const TableMeta &table_meta() const { return table_meta_; }
    
    /**
     * @brief 获取表名
     */
    const char *name() const { return table_meta_.name(); }
    
    // ...
};
```

#### 2.3 TableMeta 类：查找字段

```cpp
// src/observer/storage/table/table_meta.h
class TableMeta {
public:
    /**
     * @brief 根据字段名查找字段元数据
     * @param name 字段名
     * @return FieldMeta* 字段元数据，不存在返回 nullptr
     */
    const FieldMeta *field(const char *name) const;
    
    /**
     * @brief 获取字段数量（不含系统字段）
     */
    int field_num() const { return fields_.size(); }
    
    /**
     * @brief 获取系统字段数量
     */
    int sys_field_num() const { return sys_fields_.size(); }
    
    // ...
};
```

---

## 第3章：核心实现详解

### 🎯 校验流程总览

```
SelectStmt::create() 方法
    │
    ├─→ 步骤1：校验 FROM 子句中的表
    │       遍历 relations 列表
    │       调用 db->find_table(table_name)
    │       失败则返回 RC::SCHEMA_TABLE_NOT_EXIST
    │
    ├─→ 步骤2：校验 SELECT 列表中的字段
    │       遍历 attributes 列表
    │       调用 table_meta->field(field_name)
    │       失败则返回 RC::SCHEMA_FIELD_NOT_EXIST
    │
    ├─→ 步骤3：校验 WHERE 条件中的字段
    │       遍历 conditions 列表
    │       对每个条件的左右操作数进行字段校验
    │       失败则返回 RC::SCHEMA_FIELD_NOT_EXIST
    │
    └─→ 步骤4：创建 SelectStmt 对象
            将字符串转换为对象指针
```

### 📝 步骤1：校验表是否存在

```cpp
/**
 * 校验表名是否存在
 * @param db 当前数据库
 * @param table_name 表名
 * @param table 输出参数：表对象指针
 * @return RC::SUCCESS 或 RC::SCHEMA_TABLE_NOT_EXIST
 */
RC check_table_exist(Db *db, const char *table_name, Table *&table)
{
    // 调用数据库的 find_table 方法
    table = db->find_table(table_name);
    
    // 判断是否找到
    if (nullptr == table) {
        LOG_WARN("No such table. db=%s, table_name=%s", 
                 db->name(), table_name);
        return RC::SCHEMA_TABLE_NOT_EXIST;
    }
    
    return RC::SUCCESS;
}
```

**内部实现原理**：

```cpp
// db.cpp 中的 find_table 实现
Table *Db::find_table(const char *table_name) const
{
    // tables_ 是一个 map<string, Table*>
    auto iter = tables_.find(table_name);
    
    if (iter == tables_.end()) {
        return nullptr;  // 表不存在
    }
    
    return iter->second;  // 返回表对象
}
```

### 📝 步骤2：校验字段是否存在

```cpp
/**
 * 校验字段是否存在于指定表中
 * @param table 表对象
 * @param field_name 字段名
 * @param field_meta 输出参数：字段元数据
 * @return RC::SUCCESS 或 RC::SCHEMA_FIELD_NOT_EXIST
 */
RC check_field_exist(Table *table, const char *field_name, 
                     const FieldMeta *&field_meta)
{
    // 获取表的元数据
    const TableMeta &table_meta = table->table_meta();
    
    // 查找字段
    field_meta = table_meta.field(field_name);
    
    if (nullptr == field_meta) {
        LOG_WARN("No such field. table=%s, field=%s", 
                 table->name(), field_name);
        return RC::SCHEMA_FIELD_NOT_EXIST;
    }
    
    return RC::SUCCESS;
}
```

**内部实现原理**：

```cpp
// table_meta.cpp 中的 field 实现
const FieldMeta *TableMeta::field(const char *name) const
{
    // 先在用户字段中查找
    for (const FieldMeta &field : fields_) {
        if (0 == strcmp(field.name(), name)) {
            return &field;
        }
    }
    
    // 再在系统字段中查找
    for (const FieldMeta &field : sys_fields_) {
        if (0 == strcmp(field.name(), name)) {
            return &field;
        }
    }
    
    return nullptr;  // 字段不存在
}
```

### 📝 步骤3：处理通配符 `*`

```cpp
/**
 * 处理 SELECT * 的情况
 * 将 * 展开为表的所有字段
 */
RC wildcard_fields(Table *table, std::vector<Field> &query_fields)
{
    const TableMeta &table_meta = table->table_meta();
    
    // 获取字段数量
    const int field_num = table_meta.field_num();
    const int sys_field_num = table_meta.sys_field_num();
    
    // 遍历所有用户字段（跳过系统字段）
    for (int i = sys_field_num; i < sys_field_num + field_num; i++) {
        const FieldMeta *field_meta = table_meta.field(i);
        
        // 创建 Field 对象并添加到结果列表
        query_fields.emplace_back(table, field_meta);
    }
    
    return RC::SUCCESS;
}
```

### 📝 步骤4：处理多表查询的字段归属

```cpp
/**
 * 在多表查询中，确定字段属于哪个表
 * 
 * 例如：SELECT name FROM t1, t2;
 * 如果 t1 和 t2 都有 name 字段，需要报错（歧义）
 * 如果只有一个表有 name 字段，则确定归属
 */
RC resolve_field_table(
    const std::vector<Table *> &tables,
    const char *field_name,
    Table *&out_table,
    const FieldMeta *&out_field_meta)
{
    Table *found_table = nullptr;
    const FieldMeta *found_field = nullptr;
    
    // 遍历所有表
    for (Table *table : tables) {
        const FieldMeta *field_meta = table->table_meta().field(field_name);
        
        if (field_meta != nullptr) {
            // 找到了！
            if (found_table != nullptr) {
                // 已经在其他表中找到过，存在歧义！
                LOG_WARN("Ambiguous field name: %s", field_name);
                return RC::SCHEMA_FIELD_AMBIGUOUS;  // 字段名歧义
            }
            
            found_table = table;
            found_field = field_meta;
        }
    }
    
    if (found_table == nullptr) {
        // 所有表中都没找到
        LOG_WARN("No such field: %s", field_name);
        return RC::SCHEMA_FIELD_NOT_EXIST;
    }
    
    out_table = found_table;
    out_field_meta = found_field;
    return RC::SUCCESS;
}
```

---

## 第4章：完整代码实现

### 📝 SelectStmt::create 完整实现

```cpp
// src/observer/sql/stmt/select_stmt.cpp

RC SelectStmt::create(Db *db, const SelectSqlNode &select_sql, Stmt *&stmt)
{
    // ========== 第1步：参数校验 ==========
    if (nullptr == db) {
        LOG_WARN("Invalid argument: db is null");
        return RC::INVALID_ARGUMENT;
    }

    // ========== 第2步：收集并校验所有表 ==========
    std::vector<Table *> tables;
    std::unordered_map<std::string, Table *> table_map;  // 用于快速查找
    
    // 遍历 FROM 子句中的所有表名
    for (size_t i = 0; i < select_sql.relations.size(); i++) {
        const char *table_name = select_sql.relations[i].c_str();
        
        // 🎯 关键校验点1：表是否存在
        Table *table = db->find_table(table_name);
        if (nullptr == table) {
            LOG_WARN("No such table: %s", table_name);
            return RC::SCHEMA_TABLE_NOT_EXIST;  // ← 返回表不存在错误
        }
        
        // 检查表名是否重复
        if (table_map.count(table_name) > 0) {
            LOG_WARN("Duplicate table name: %s", table_name);
            return RC::SCHEMA_TABLE_DUPLICATED;
        }
        
        tables.push_back(table);
        table_map[table_name] = table;
    }
    
    // 如果没有指定表，使用默认表（某些数据库支持）
    // MiniOB 要求必须有表
    if (tables.empty()) {
        LOG_WARN("No table specified in FROM clause");
        return RC::SCHEMA_TABLE_NOT_EXIST;
    }

    // ========== 第3步：收集并校验 SELECT 列表中的字段 ==========
    std::vector<Field> query_fields;
    
    for (int i = static_cast<int>(select_sql.attributes.size()) - 1; i >= 0; i--) {
        const RelAttrSqlNode &relation_attr = select_sql.attributes[i];
        
        // 3.1 处理 SELECT *
        if (common::is_blank(relation_attr.attribute_name.c_str()) ||
            0 == strcmp(relation_attr.attribute_name.c_str(), "*")) {
            
            // 没有指定表名：SELECT *
            if (common::is_blank(relation_attr.relation_name.c_str())) {
                // 展开所有表的所有字段
                for (Table *table : tables) {
                    wildcard_fields(table, query_fields);
                }
            } 
            // 指定了表名：SELECT t1.*
            else {
                const char *table_name = relation_attr.relation_name.c_str();
                
                // 🎯 关键校验点2：指定的表是否在 FROM 中
                auto iter = table_map.find(table_name);
                if (iter == table_map.end()) {
                    LOG_WARN("Table not in FROM clause: %s", table_name);
                    return RC::SCHEMA_TABLE_NOT_EXIST;
                }
                
                wildcard_fields(iter->second, query_fields);
            }
        }
        // 3.2 处理具体字段名
        else {
            const char *field_name = relation_attr.attribute_name.c_str();
            
            // 没有指定表名：SELECT name
            if (common::is_blank(relation_attr.relation_name.c_str())) {
                // 需要在所有表中查找
                if (tables.size() == 1) {
                    // 单表查询，直接在该表中查找
                    Table *table = tables[0];
                    const FieldMeta *field_meta = table->table_meta().field(field_name);
                    
                    // 🎯 关键校验点3：字段是否存在
                    if (nullptr == field_meta) {
                        LOG_WARN("No such field: %s.%s", table->name(), field_name);
                        return RC::SCHEMA_FIELD_NOT_EXIST;
                    }
                    
                    query_fields.emplace_back(table, field_meta);
                } 
                else {
                    // 多表查询，需要确定字段属于哪个表
                    Table *found_table = nullptr;
                    const FieldMeta *found_field = nullptr;
                    
                    RC rc = resolve_field_table(tables, field_name, 
                                                found_table, found_field);
                    if (rc != RC::SUCCESS) {
                        return rc;  // 字段不存在或歧义
                    }
                    
                    query_fields.emplace_back(found_table, found_field);
                }
            }
            // 指定了表名：SELECT t1.name
            else {
                const char *table_name = relation_attr.relation_name.c_str();
                
                // 🎯 关键校验点4：表是否在 FROM 中
                auto iter = table_map.find(table_name);
                if (iter == table_map.end()) {
                    LOG_WARN("Table not in FROM clause: %s", table_name);
                    return RC::SCHEMA_TABLE_NOT_EXIST;
                }
                
                Table *table = iter->second;
                const FieldMeta *field_meta = table->table_meta().field(field_name);
                
                // 🎯 关键校验点5：字段是否存在于指定表
                if (nullptr == field_meta) {
                    LOG_WARN("No such field: %s.%s", table_name, field_name);
                    return RC::SCHEMA_FIELD_NOT_EXIST;
                }
                
                query_fields.emplace_back(table, field_meta);
            }
        }
    }

    // ========== 第4步：校验 WHERE 条件中的字段 ==========
    FilterStmt *filter_stmt = nullptr;
    
    if (!select_sql.conditions.empty()) {
        RC rc = FilterStmt::create(
            db,
            tables[0],           // 默认表（单表查询）
            &table_map,          // 表名映射
            select_sql.conditions.data(),
            static_cast<int>(select_sql.conditions.size()),
            filter_stmt
        );
        
        if (rc != RC::SUCCESS) {
            LOG_WARN("Failed to create filter statement");
            return rc;  // 可能是字段不存在
        }
    }

    // ========== 第5步：创建 SelectStmt 对象 ==========
    SelectStmt *select_stmt = new SelectStmt();
    select_stmt->tables_.swap(tables);
    select_stmt->query_fields_.swap(query_fields);
    select_stmt->filter_stmt_ = filter_stmt;
    
    stmt = select_stmt;
    
    LOG_INFO("SelectStmt created successfully. table_num=%d, field_num=%d",
             static_cast<int>(select_stmt->tables_.size()),
             static_cast<int>(select_stmt->query_fields_.size()));
    
    return RC::SUCCESS;
}
```

### 📝 FilterStmt::create 中的字段校验

```cpp
// src/observer/sql/stmt/filter_stmt.cpp

RC FilterStmt::create(
    Db *db,
    Table *default_table,
    std::unordered_map<std::string, Table *> *tables,
    const ConditionSqlNode *conditions,
    int condition_num,
    FilterStmt *&stmt)
{
    RC rc = RC::SUCCESS;
    std::vector<FilterUnit *> filter_units;
    
    // 遍历所有条件
    for (int i = 0; i < condition_num; i++) {
        const ConditionSqlNode &condition = conditions[i];
        FilterUnit *filter_unit = nullptr;
        
        // 创建过滤单元（会进行字段校验）
        rc = create_filter_unit(db, default_table, tables, condition, filter_unit);
        
        if (rc != RC::SUCCESS) {
            // 清理已创建的资源
            for (FilterUnit *unit : filter_units) {
                delete unit;
            }
            return rc;  // ← 字段校验失败
        }
        
        filter_units.push_back(filter_unit);
    }
    
    stmt = new FilterStmt();
    stmt->filter_units_.swap(filter_units);
    
    return RC::SUCCESS;
}

/**
 * 创建单个过滤条件
 * 这里会校验条件中的字段是否存在
 */
RC FilterStmt::create_filter_unit(
    Db *db,
    Table *default_table,
    std::unordered_map<std::string, Table *> *tables,
    const ConditionSqlNode &condition,
    FilterUnit *&filter_unit)
{
    // 处理左操作数
    Expression *left_expr = nullptr;
    
    if (condition.left_is_attr) {
        // 左边是字段，需要校验
        Table *table = nullptr;
        const FieldMeta *field = nullptr;
        
        // 🎯 校验字段
        RC rc = get_table_and_field(
            db, default_table, tables,
            condition.left_attr,
            table, field
        );
        
        if (rc != RC::SUCCESS) {
            return rc;  // 字段不存在
        }
        
        left_expr = new FieldExpr(table, field);
    } else {
        // 左边是值，不需要校验
        left_expr = new ValueExpr(condition.left_value);
    }
    
    // 处理右操作数（类似逻辑）
    Expression *right_expr = nullptr;
    
    if (condition.right_is_attr) {
        Table *table = nullptr;
        const FieldMeta *field = nullptr;
        
        RC rc = get_table_and_field(
            db, default_table, tables,
            condition.right_attr,
            table, field
        );
        
        if (rc != RC::SUCCESS) {
            delete left_expr;  // 清理资源
            return rc;
        }
        
        right_expr = new FieldExpr(table, field);
    } else {
        right_expr = new ValueExpr(condition.right_value);
    }
    
    // 创建过滤单元
    filter_unit = new FilterUnit();
    filter_unit->set_left(left_expr);
    filter_unit->set_right(right_expr);
    filter_unit->set_comp(condition.comp);
    
    return RC::SUCCESS;
}

/**
 * 根据字段描述获取表和字段元数据
 */
RC FilterStmt::get_table_and_field(
    Db *db,
    Table *default_table,
    std::unordered_map<std::string, Table *> *tables,
    const RelAttrSqlNode &attr,
    Table *&table,
    const FieldMeta *&field)
{
    // 如果指定了表名
    if (!common::is_blank(attr.relation_name.c_str())) {
        // 在表映射中查找
        auto iter = tables->find(attr.relation_name);
        
        if (iter == tables->end()) {
            // 🎯 表不在 FROM 子句中
            LOG_WARN("Table not found: %s", attr.relation_name.c_str());
            return RC::SCHEMA_TABLE_NOT_EXIST;
        }
        
        table = iter->second;
    } else {
        // 没有指定表名，使用默认表
        table = default_table;
    }
    
    // 在表中查找字段
    field = table->table_meta().field(attr.attribute_name.c_str());
    
    if (nullptr == field) {
        // 🎯 字段不存在
        LOG_WARN("Field not found: %s.%s", 
                 table->name(), attr.attribute_name.c_str());
        return RC::SCHEMA_FIELD_NOT_EXIST;
    }
    
    return RC::SUCCESS;
}
```

---

## 第5章：调试实践

### 🔍 调试断点设置

```cpp
// 断点1：表校验入口
文件：src/observer/sql/stmt/select_stmt.cpp
位置：SelectStmt::create 方法开始处
目的：查看传入的 ParsedSqlNode 内容

// 断点2：表查找
文件：src/observer/storage/db/db.cpp
位置：Db::find_table 方法
目的：观察表查找过程

// 断点3：字段查找
文件：src/observer/storage/table/table_meta.cpp
位置：TableMeta::field 方法
目的：观察字段查找过程

// 断点4：返回错误
文件：src/observer/sql/stmt/select_stmt.cpp
位置：return RC::SCHEMA_TABLE_NOT_EXIST; 或 
      return RC::SCHEMA_FIELD_NOT_EXIST;
目的：确认错误是否正确返回
```

### 🐛 调试步骤

#### 场景1：测试表不存在

```sql
-- 1. 启动调试（F5）
-- 2. 输入 SQL
SELECT * FROM not_exist_table;

-- 3. 断点会停在 SelectStmt::create
-- 4. 查看变量
select_sql.relations[0] = "not_exist_table"

-- 5. 单步执行到 db->find_table
table = db->find_table("not_exist_table");
// table = nullptr

-- 6. 继续执行
// 返回 RC::SCHEMA_TABLE_NOT_EXIST

-- 7. 客户端看到
FAILURE
```

#### 场景2：测试字段不存在

```sql
-- 准备工作
CREATE TABLE test (id INT, name CHAR(10));

-- 测试
SELECT salary FROM test;

-- 调试观察
-- 1. table = db->find_table("test") 成功，table != nullptr
-- 2. field_meta = table_meta.field("salary") 失败，field_meta = nullptr
-- 3. 返回 RC::SCHEMA_FIELD_NOT_EXIST
```

### 📊 变量观察

```cpp
// 在调试器中查看这些变量

// 1. 查看输入的 SQL 解析结果
select_sql.relations      // FROM 子句中的表名列表
select_sql.attributes     // SELECT 列表中的字段
select_sql.conditions     // WHERE 条件

// 2. 查看表对象
table->name()            // 表名
table->table_meta()      // 表元数据

// 3. 查看字段元数据
field_meta->name()       // 字段名
field_meta->type()       // 字段类型
field_meta->offset()     // 字段在记录中的偏移量

// 4. 查看错误码
rc                       // 当前的返回码
strrc(rc)               // 返回码的字符串表示
```

---

## 第6章：测试用例

### 📝 测试用例清单

```sql
-- ========== 准备工作 ==========
CREATE TABLE students (id INT, name CHAR(20), age INT);
CREATE TABLE courses (cid INT, cname CHAR(30));

-- ========== 测试1：表不存在 ==========
SELECT * FROM not_exist_table;
-- 预期：FAILURE (SCHEMA_TABLE_NOT_EXIST)

-- ========== 测试2：字段不存在 ==========
SELECT salary FROM students;
-- 预期：FAILURE (SCHEMA_FIELD_NOT_EXIST)

-- ========== 测试3：正常查询 ==========
SELECT id, name FROM students;
-- 预期：SUCCESS

-- ========== 测试4：带表名前缀的字段 ==========
SELECT students.id, students.name FROM students;
-- 预期：SUCCESS

-- ========== 测试5：SELECT * ==========
SELECT * FROM students;
-- 预期：SUCCESS，展开所有字段

-- ========== 测试6：多表查询 ==========
SELECT * FROM students, courses;
-- 预期：SUCCESS

-- ========== 测试7：多表查询中字段不存在 ==========
SELECT students.salary FROM students, courses;
-- 预期：FAILURE (SCHEMA_FIELD_NOT_EXIST)

-- ========== 测试8：WHERE 条件中字段不存在 ==========
SELECT * FROM students WHERE salary > 1000;
-- 预期：FAILURE (SCHEMA_FIELD_NOT_EXIST)

-- ========== 测试9：WHERE 条件中表名不存在 ==========
SELECT * FROM students WHERE other_table.id = 1;
-- 预期：FAILURE (SCHEMA_TABLE_NOT_EXIST)

-- ========== 测试10：表名前缀指向不在 FROM 中的表 ==========
SELECT courses.cid FROM students;
-- 预期：FAILURE (表 courses 不在 FROM 子句中)
```

### ✅ 测试结果验证

```
测试用例        | 预期结果                    | 实际结果
---------------|----------------------------|----------
表不存在        | FAILURE                    | FAILURE ✓
字段不存在      | FAILURE                    | FAILURE ✓
正常查询        | SUCCESS + 数据             | SUCCESS ✓
SELECT *       | SUCCESS + 展开字段          | SUCCESS ✓
多表查询        | SUCCESS                    | SUCCESS ✓
WHERE字段错误   | FAILURE                    | FAILURE ✓
```

---

## 第7章：常见问题与解决

### ❓ Q1：为什么有时候字段存在但报错？

**可能原因**：
1. **大小写问题**：MiniOB 的字段名是大小写敏感的
2. **空格问题**：字段名前后有空格
3. **多表歧义**：多表查询时同名字段未指定表名

**解决方法**：
```cpp
// 确保字段名比较时处理大小写
// 在 table_meta.cpp 中
if (0 == strcasecmp(field.name(), name)) {  // 忽略大小写
    return &field;
}
```

### ❓ Q2：如何支持表别名？

```sql
-- 目前不支持
SELECT t.id FROM students t;  -- t 是别名

-- 需要在 select_sql 中增加别名字段
-- 并在校验时使用别名映射
```

### ❓ Q3：如何处理聚合函数中的字段？

```sql
SELECT COUNT(id) FROM students;

-- COUNT(id) 中的 id 也需要校验
-- 在处理聚合表达式时递归校验字段
```

### 🔧 常见错误及修复

```cpp
// 错误1：忘记校验 FROM 子句为空的情况
// 修复：
if (select_sql.relations.empty()) {
    return RC::SQL_SYNTAX;  // 或其他合适的错误码
}

// 错误2：多表查询时没有处理字段歧义
// 修复：
if (found_table != nullptr && field_meta != nullptr) {
    // 已经找到过，存在歧义
    return RC::SCHEMA_FIELD_AMBIGUOUS;
}

// 错误3：SELECT * 时没有正确展开字段
// 修复：确保遍历所有表，且跳过系统字段
for (int i = sys_field_num; i < total_field_num; i++) {
    // ...
}
```

---

## 第8章：总结

### 🎯 核心要点

1. **元数据校验发生在 Resolver 阶段**
   - Parser 只检查语法
   - Resolver 检查语义（表/字段是否存在）

2. **校验顺序**
   - 先校验表（FROM 子句）
   - 再校验字段（SELECT / WHERE / ORDER BY / GROUP BY）

3. **关键方法**
   - `Db::find_table()` - 查找表
   - `TableMeta::field()` - 查找字段

4. **错误码**
   - `RC::SCHEMA_TABLE_NOT_EXIST` - 表不存在
   - `RC::SCHEMA_FIELD_NOT_EXIST` - 字段不存在

### 📝 实现检查清单

- [ ] FROM 子句中的表校验
- [ ] SELECT 列表中的字段校验
- [ ] WHERE 条件中的字段校验
- [ ] 带表名前缀的字段校验
- [ ] SELECT * 的展开
- [ ] 多表查询的字段归属
- [ ] 错误信息的日志记录

---

**恭喜！你已经掌握了 SELECT-META 功能的实现！** 🎉

