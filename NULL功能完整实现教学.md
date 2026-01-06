# 🎓 NULL 功能完整实现教学

> 空值支持：从存储格式到三值逻辑的完整实现

## 📋 目录

- [第0章：准备知识](#第0章准备知识)
- [第1章：功能需求分析](#第1章功能需求分析)
- [第2阶段：Parser 解析](#第2阶段parser-解析)
- [第3阶段：存储格式设计](#第3阶段存储格式设计)
- [第4阶段：Resolver 实现](#第4阶段resolver-实现)
- [第5阶段：比较逻辑实现](#第5阶段比较逻辑实现)
- [第6阶段：聚合函数处理](#第6阶段聚合函数处理)
- [第7章：调试实践](#第7章调试实践)
- [第8章：测试用例](#第8章测试用例)

---

## 第0章：准备知识

### 📝 什么是 NULL？

NULL 表示"未知"或"缺失"的值，不同于空字符串或零。

**示例**：
```sql
-- 创建允许 NULL 的字段
CREATE TABLE students (
    id INT NOT NULL,           -- 不允许 NULL
    name CHAR(20) NOT NULL,    -- 不允许 NULL
    age INT NULLABLE           -- 允许 NULL
);

-- 插入 NULL 值
INSERT INTO students VALUES (1, 'Alice', NULL);
INSERT INTO students VALUES (2, 'Bob', 20);

-- 查询 NULL
SELECT * FROM students WHERE age IS NULL;
SELECT * FROM students WHERE age IS NOT NULL;
```

### 🎯 实现目标

1. ✅ 建表时支持 `NULLABLE` / `NOT NULL` 关键字
2. ✅ 插入数据时支持 `NULL` 值
3. ✅ 存储层支持 NULL 标记
4. ✅ 查询支持 `IS NULL` / `IS NOT NULL`
5. ✅ NULL 比较遵循 SQL 标准（三值逻辑）
6. ✅ 聚合函数正确处理 NULL

### ⚠️ NULL 的特殊规则（SQL 标准）

```
1. NULL 与任何值的比较结果都是 UNKNOWN（在 WHERE 中视为 FALSE）
   - NULL = NULL  → UNKNOWN (FALSE)
   - NULL <> NULL → UNKNOWN (FALSE)
   - NULL > 10    → UNKNOWN (FALSE)
   - NULL = 10    → UNKNOWN (FALSE)

2. 只有 IS NULL 和 IS NOT NULL 能正确判断 NULL
   - NULL IS NULL     → TRUE
   - NULL IS NOT NULL → FALSE
   - 10 IS NULL       → FALSE

3. 聚合函数忽略 NULL 值
   - COUNT(col)：不计 NULL
   - SUM(col)：跳过 NULL
   - AVG(col)：不算 NULL
   - MAX/MIN(col)：忽略 NULL
```

---

## 第1章：功能需求分析

### 📊 存储格式设计

```
原来的记录格式：
[字段1数据] [字段2数据] [字段3数据] ...

支持 NULL 后的格式：
[NULL位图] [字段1数据] [字段2数据] [字段3数据] ...
    ↑
    每个字段对应1位
    0 = 非NULL
    1 = NULL
```

**示例**：
```
表结构：students(id INT NOT NULL, name CHAR(10) NOT NULL, age INT NULLABLE)

记录：(1, 'Alice', NULL)

二进制格式：
[NULL位图: 00000100] [id: 01000000] [name: Alice...] [age: ????????]
           ↑                                              ↑
        第3位=1，表示第3个字段是NULL            这部分数据无效

注意：NULL 字段仍然占用空间，但内容无意义
```

### 📋 MiniOB 的 NULL 规则

```cpp
// MiniOB 与 MySQL 的区别：
// MySQL: 默认允许 NULL，用 NOT NULL 禁止
// MiniOB: 默认不允许 NULL，用 NULLABLE 允许

// 语法示例
CREATE TABLE t (
    id INT,                    // 默认 NOT NULL
    age INT NOT NULL,          // 显式 NOT NULL
    name CHAR(10) NULLABLE     // 允许 NULL
);
```

---

## 第2阶段：Parser 解析

### 📁 涉及的文件

```
src/observer/sql/parser/
├── lex_sql.l        ← 词法：NULL, NULLABLE, IS 关键字
├── yacc_sql.y       ← 语法：字段定义、NULL 值、IS NULL 条件
└── parse_defs.h     ← 数据结构
```

### 📝 2.1 词法分析（lex_sql.l）

```c
/* NULL 相关关键字 */
"NULL"       { return NULL_T; }
"NULLABLE"   { return NULLABLE; }
"NOT"        { return NOT; }
"IS"         { return IS; }

%%
```

### 📝 2.2 语法分析（yacc_sql.y）

```yacc
/* Token 声明 */
%token NULL_T NULLABLE NOT IS

%%

/* ========== 建表时的字段定义 ========== */
attr_def:
    ID type LBRACE NUMBER RBRACE null_option
    {
        // 带长度的类型，如 CHAR(20)
        $$ = new AttrInfoSqlNode;
        $$->name = $1;
        $$->type = static_cast<AttrType>($2);
        $$->length = $4;
        $$->nullable = $6;  // ← NULL 选项
        free($1);
    }
    | ID type null_option
    {
        // 不带长度的类型，如 INT
        $$ = new AttrInfoSqlNode;
        $$->name = $1;
        $$->type = static_cast<AttrType>($2);
        $$->length = 4;  // 默认长度
        $$->nullable = $3;  // ← NULL 选项
        free($1);
    }
    ;

/* NULL 选项 */
null_option:
    /* empty */
    {
        $$ = false;  // 默认不允许 NULL
    }
    | NOT NULL_T
    {
        $$ = false;  // 显式不允许 NULL
    }
    | NULLABLE
    {
        $$ = true;   // 允许 NULL
    }
    ;

/* ========== INSERT 中的 NULL 值 ========== */
value:
    NUMBER {
        $$ = new Value();
        $$->set_int($1);
    }
    | NULL_T {
        // ← 新增：NULL 值
        $$ = new Value();
        $$->set_null();
    }
    /* ... 其他类型 ... */
    ;

/* ========== WHERE 中的 IS NULL 条件 ========== */
condition:
    /* 普通比较条件 */
    expression comp_op expression
    {
        $$ = new ConditionSqlNode;
        $$->left_expr = $1;
        $$->comp = $2;
        $$->right_expr = $3;
    }
    /* IS NULL 条件 */
    | expression IS NULL_T
    {
        $$ = new ConditionSqlNode;
        $$->left_expr = $1;
        $$->comp = CompOp::IS_NULL;
        $$->right_expr = nullptr;  // 右边没有表达式
    }
    /* IS NOT NULL 条件 */
    | expression IS NOT NULL_T
    {
        $$ = new ConditionSqlNode;
        $$->left_expr = $1;
        $$->comp = CompOp::IS_NOT_NULL;
        $$->right_expr = nullptr;
    }
    ;

%%
```

### 📝 2.3 数据结构定义

```cpp
// src/observer/sql/parser/parse_defs.h

/**
 * @brief 字段定义信息
 */
struct AttrInfoSqlNode {
    std::string name;      // 字段名
    AttrType type;         // 字段类型
    int length;            // 长度
    bool nullable;         // ← 新增：是否允许 NULL
};

/**
 * @brief 比较操作符
 */
enum class CompOp {
    EQUAL_TO,       // =
    NOT_EQUAL,      // <> 或 !=
    LESS_THAN,      // <
    GREAT_THAN,     // >
    LESS_EQUAL,     // <=
    GREAT_EQUAL,    // >=
    IS_NULL,        // ← 新增：IS NULL
    IS_NOT_NULL,    // ← 新增：IS NOT NULL
    // ...
};
```

---

## 第3阶段：存储格式设计

### 📝 3.1 NULL 位图设计

```cpp
// src/observer/storage/table/table_meta.h

class TableMeta {
public:
    /**
     * @brief 获取 NULL 位图的大小（字节数）
     * 每个字段占1位，向上取整到字节
     */
    int null_bitmap_size() const {
        int field_num = fields_.size();
        return (field_num + 7) / 8;
    }
    
    /**
     * @brief 获取第 i 个字段是否允许 NULL
     */
    bool field_nullable(int index) const {
        return fields_[index].nullable();
    }
    
    // ...
};
```

### 📝 3.2 记录格式变更

```cpp
// 记录格式：
// [NULL位图 (N bytes)] + [字段1数据] + [字段2数据] + ...
// 
// 其中 N = (字段数 + 7) / 8

// 示例：3个字段
// N = (3 + 7) / 8 = 1 字节
// 
// 位图格式（1字节 = 8位）：
// [bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0]
//                          ↑    ↑    ↑
//                        字段3 字段2 字段1
//                        
// 如果字段2是NULL，位图 = 00000010
```

### 📝 3.3 FieldMeta 修改

```cpp
// src/observer/storage/field/field_meta.h

class FieldMeta {
public:
    // ... 原有方法 ...
    
    /**
     * @brief 是否允许 NULL
     */
    bool nullable() const { return nullable_; }
    
    /**
     * @brief 设置是否允许 NULL
     */
    void set_nullable(bool nullable) { nullable_ = nullable; }

private:
    // ... 原有成员 ...
    bool nullable_ = false;  // ← 新增
};
```

### 📝 3.4 计算字段偏移量

```cpp
// src/observer/storage/table/table_meta.cpp

/**
 * @brief 初始化表元数据
 * 计算每个字段的偏移量（考虑 NULL 位图）
 */
RC TableMeta::init(const char *name, 
                   int attr_count, 
                   const AttrInfoSqlNode *attrs)
{
    name_ = name;
    
    // ========== 步骤1：计算 NULL 位图大小 ==========
    int bitmap_size = (attr_count + 7) / 8;
    
    // ========== 步骤2：计算字段偏移量 ==========
    int offset = bitmap_size;  // 从位图之后开始
    
    for (int i = 0; i < attr_count; i++) {
        FieldMeta field;
        
        // 设置字段属性
        field.init(
            attrs[i].name.c_str(),
            attrs[i].type,
            offset,
            attrs[i].length,
            attrs[i].nullable  // ← 传递 nullable 属性
        );
        
        fields_.push_back(field);
        
        // 更新偏移量
        offset += attrs[i].length;
    }
    
    // ========== 步骤3：记录总大小 ==========
    record_size_ = offset;
    
    return RC::SUCCESS;
}
```

---

## 第4阶段：Resolver 实现

### 📝 4.1 INSERT 语句校验

```cpp
// src/observer/sql/stmt/insert_stmt.cpp

RC InsertStmt::create(Db *db, const InsertSqlNode &inserts, Stmt *&stmt)
{
    // ... 前面的代码 ...
    
    // ========== 校验每一行数据 ==========
    for (auto &values : inserts.tuples) {
        
        // 遍历每个字段
        for (int i = 0; i < field_num; i++) {
            const FieldMeta *field_meta = table_meta.field(i + sys_field_num);
            const Value &value = values[i];
            
            // ========== 新增：NULL 值校验 ==========
            if (value.is_null()) {
                // 检查字段是否允许 NULL
                if (!field_meta->nullable()) {
                    LOG_WARN("Field '%s' does not allow NULL", 
                             field_meta->name());
                    return RC::SCHEMA_FIELD_NOT_NULL;  // ← 新错误码
                }
                
                // NULL 值不需要类型检查，直接跳过
                continue;
            }
            
            // ========== 原有的类型校验 ==========
            AttrType field_type = field_meta->type();
            AttrType value_type = value.attr_type();
            
            if (field_type != value_type) {
                // 尝试类型转换...
            }
        }
    }
    
    // ... 后面的代码 ...
}
```

### 📝 4.2 Value 类修改

```cpp
// src/observer/common/value.h

class Value {
public:
    /**
     * @brief 设置为 NULL 值
     */
    void set_null() {
        type_ = AttrType::NULLS;  // 特殊类型标记
        is_null_ = true;
    }
    
    /**
     * @brief 判断是否为 NULL
     */
    bool is_null() const {
        return is_null_;
    }

private:
    AttrType type_ = AttrType::UNDEFINED;
    bool is_null_ = false;  // ← 新增 NULL 标记
    
    // ... 其他成员 ...
};
```

### 📝 4.3 创建记录时处理 NULL

```cpp
// src/observer/storage/table/table.cpp

RC Table::make_record(int value_num, const Value *values, Record &record)
{
    // ========== 步骤1：计算记录大小 ==========
    int record_size = table_meta_.record_size();
    int bitmap_size = table_meta_.null_bitmap_size();
    
    // ========== 步骤2：分配内存 ==========
    char *record_data = (char *)malloc(record_size);
    memset(record_data, 0, record_size);  // 初始化为0
    
    // ========== 步骤3：设置 NULL 位图 ==========
    // 位图在记录开头
    char *bitmap = record_data;
    
    const int sys_field_num = table_meta_.sys_field_num();
    
    for (int i = 0; i < value_num; i++) {
        const Value &value = values[i];
        
        if (value.is_null()) {
            // 设置位图中对应的位
            int byte_index = i / 8;
            int bit_index = i % 8;
            bitmap[byte_index] |= (1 << bit_index);
            
            // NULL 字段不需要复制数据，跳过
            continue;
        }
        
        // ========== 步骤4：复制非 NULL 字段的数据 ==========
        const FieldMeta *field = table_meta_.field(i + sys_field_num);
        size_t offset = field->offset();
        size_t len = field->len();
        
        switch (field->type()) {
            case AttrType::INTS:
            {
                int32_t int_value = value.get_int();
                memcpy(record_data + offset, &int_value, sizeof(int32_t));
                break;
            }
            case AttrType::FLOATS:
            {
                float float_value = value.get_float();
                memcpy(record_data + offset, &float_value, sizeof(float));
                break;
            }
            case AttrType::CHARS:
            {
                const char *str = value.get_string().c_str();
                size_t str_len = strlen(str);
                if (str_len > len) str_len = len;
                memcpy(record_data + offset, str, str_len);
                break;
            }
            // ... 其他类型 ...
        }
    }
    
    // ========== 步骤5：设置记录对象 ==========
    record.set_data_owner(record_data, record_size);
    
    return RC::SUCCESS;
}
```

### 📝 4.4 读取记录时处理 NULL

```cpp
// src/observer/storage/table/table.cpp

/**
 * @brief 从记录中读取指定字段的值
 */
RC Table::get_value(const Record &record, int field_index, Value &value)
{
    const TableMeta &meta = table_meta_;
    const FieldMeta *field = meta.field(field_index);
    
    const char *record_data = record.data();
    
    // ========== 步骤1：检查 NULL 位图 ==========
    int bitmap_byte = field_index / 8;
    int bitmap_bit = field_index % 8;
    
    if (record_data[bitmap_byte] & (1 << bitmap_bit)) {
        // 该字段是 NULL
        value.set_null();
        return RC::SUCCESS;
    }
    
    // ========== 步骤2：读取非 NULL 值 ==========
    size_t offset = field->offset();
    const char *data = record_data + offset;
    
    switch (field->type()) {
        case AttrType::INTS:
            value.set_int(*(int32_t *)data);
            break;
        case AttrType::FLOATS:
            value.set_float(*(float *)data);
            break;
        case AttrType::CHARS:
            value.set_string(data, field->len());
            break;
        // ... 其他类型 ...
    }
    
    return RC::SUCCESS;
}
```

---

## 第5阶段：比较逻辑实现

### 📝 5.1 三值逻辑

```
SQL 的三值逻辑（Three-Valued Logic）：

TRUE, FALSE, UNKNOWN

真值表：
AND    | TRUE    | FALSE   | UNKNOWN
-------|---------|---------|--------
TRUE   | TRUE    | FALSE   | UNKNOWN
FALSE  | FALSE   | FALSE   | FALSE
UNKNOWN| UNKNOWN | FALSE   | UNKNOWN

OR     | TRUE    | FALSE   | UNKNOWN
-------|---------|---------|--------
TRUE   | TRUE    | TRUE    | TRUE
FALSE  | TRUE    | FALSE   | UNKNOWN
UNKNOWN| TRUE    | UNKNOWN | UNKNOWN

NOT UNKNOWN = UNKNOWN
```

### 📝 5.2 比较函数实现

```cpp
// src/observer/common/value.cpp

/**
 * @brief 比较两个值
 * @return 负数：左 < 右
 *         0：   左 == 右
 *         正数：左 > 右
 *         INT_MAX：有 NULL 参与比较（UNKNOWN）
 */
int Value::compare(const Value &other) const
{
    // ========== 处理 NULL ==========
    // 如果任一方是 NULL，返回特殊值表示 UNKNOWN
    if (this->is_null() || other.is_null()) {
        return INT_MAX;  // 特殊值，表示无法比较
    }
    
    // ========== 正常比较 ==========
    // 类型相同的情况
    if (type_ == other.type_) {
        switch (type_) {
            case AttrType::INTS:
                return int_value_ - other.int_value_;
            case AttrType::FLOATS:
                if (float_value_ < other.float_value_) return -1;
                if (float_value_ > other.float_value_) return 1;
                return 0;
            case AttrType::CHARS:
                return str_value_.compare(other.str_value_);
            // ...
        }
    }
    
    // 类型不同，需要转换后比较
    // ...
    
    return 0;
}
```

### 📝 5.3 谓词过滤中的 NULL 处理

```cpp
// src/observer/sql/operator/predicate_physical_operator.cpp

/**
 * @brief 评估过滤条件
 */
bool PredicatePhysicalOperator::evaluate(const Tuple &tuple)
{
    for (FilterUnit *unit : filter_stmt_->filter_units()) {
        // 获取左右操作数的值
        Value left_value;
        Value right_value;
        
        unit->left()->get_value(tuple, left_value);
        
        CompOp comp = unit->comp();
        
        // ========== 处理 IS NULL ==========
        if (comp == CompOp::IS_NULL) {
            if (!left_value.is_null()) {
                return false;  // 不是 NULL，条件不满足
            }
            continue;  // 是 NULL，条件满足，继续检查下一个条件
        }
        
        // ========== 处理 IS NOT NULL ==========
        if (comp == CompOp::IS_NOT_NULL) {
            if (left_value.is_null()) {
                return false;  // 是 NULL，条件不满足
            }
            continue;  // 不是 NULL，条件满足
        }
        
        // ========== 处理普通比较 ==========
        unit->right()->get_value(tuple, right_value);
        
        // 如果有 NULL 参与比较，结果为 UNKNOWN (视为 FALSE)
        if (left_value.is_null() || right_value.is_null()) {
            return false;  // ← 关键：NULL 比较返回 FALSE
        }
        
        // 正常比较
        int cmp = left_value.compare(right_value);
        
        bool result = false;
        switch (comp) {
            case CompOp::EQUAL_TO:
                result = (cmp == 0);
                break;
            case CompOp::NOT_EQUAL:
                result = (cmp != 0);
                break;
            case CompOp::LESS_THAN:
                result = (cmp < 0);
                break;
            case CompOp::GREAT_THAN:
                result = (cmp > 0);
                break;
            case CompOp::LESS_EQUAL:
                result = (cmp <= 0);
                break;
            case CompOp::GREAT_EQUAL:
                result = (cmp >= 0);
                break;
            default:
                result = false;
        }
        
        if (!result) {
            return false;
        }
    }
    
    return true;  // 所有条件都满足
}
```

---

## 第6阶段：聚合函数处理

### 📝 6.1 聚合函数中的 NULL 处理规则

```
COUNT(*):     计算所有行（包括 NULL）
COUNT(col):   只计非 NULL 的行
SUM(col):     跳过 NULL
AVG(col):     跳过 NULL（分母也不计）
MAX(col):     忽略 NULL
MIN(col):     忽略 NULL

示例：
表数据：
| id | age |
|----|-----|
| 1  | 20  |
| 2  | NULL|
| 3  | 30  |

COUNT(*) = 3
COUNT(age) = 2
SUM(age) = 50
AVG(age) = 25  (50/2，不是50/3)
MAX(age) = 30
MIN(age) = 20
```

### 📝 6.2 Aggregator 修改

```cpp
// src/observer/sql/operator/aggregator.h

/**
 * @brief 聚合器基类
 */
class Aggregator {
public:
    virtual ~Aggregator() = default;
    
    /**
     * @brief 添加一个值到聚合计算
     */
    virtual void aggregate(const Value &value) = 0;
    
    /**
     * @brief 获取聚合结果
     */
    virtual Value result() const = 0;
    
protected:
    int count_ = 0;  // 非 NULL 值的数量
};

/**
 * @brief COUNT 聚合器
 */
class CountAggregator : public Aggregator {
public:
    void aggregate(const Value &value) override {
        // COUNT(*) 或 COUNT(col)
        if (!count_star_) {
            // COUNT(col)：跳过 NULL
            if (value.is_null()) {
                return;
            }
        }
        count_++;
    }
    
    Value result() const override {
        Value v;
        v.set_int(count_);
        return v;
    }
    
    void set_count_star(bool is_star) { count_star_ = is_star; }
    
private:
    bool count_star_ = false;  // 是否是 COUNT(*)
};

/**
 * @brief SUM 聚合器
 */
class SumAggregator : public Aggregator {
public:
    void aggregate(const Value &value) override {
        // ========== 跳过 NULL ==========
        if (value.is_null()) {
            return;
        }
        
        count_++;
        
        // 累加
        if (value.attr_type() == AttrType::INTS) {
            sum_ += value.get_int();
        } else if (value.attr_type() == AttrType::FLOATS) {
            sum_ += value.get_float();
        }
    }
    
    Value result() const override {
        Value v;
        if (count_ == 0) {
            v.set_null();  // 如果没有非NULL值，结果为NULL
        } else {
            v.set_float(sum_);
        }
        return v;
    }
    
private:
    double sum_ = 0.0;
};

/**
 * @brief AVG 聚合器
 */
class AvgAggregator : public Aggregator {
public:
    void aggregate(const Value &value) override {
        // ========== 跳过 NULL ==========
        if (value.is_null()) {
            return;
        }
        
        count_++;
        
        if (value.attr_type() == AttrType::INTS) {
            sum_ += value.get_int();
        } else if (value.attr_type() == AttrType::FLOATS) {
            sum_ += value.get_float();
        }
    }
    
    Value result() const override {
        Value v;
        if (count_ == 0) {
            v.set_null();  // 没有非NULL值，结果为NULL
        } else {
            v.set_float(sum_ / count_);  // 只除以非NULL的数量
        }
        return v;
    }
    
private:
    double sum_ = 0.0;
};

/**
 * @brief MAX 聚合器
 */
class MaxAggregator : public Aggregator {
public:
    void aggregate(const Value &value) override {
        // ========== 跳过 NULL ==========
        if (value.is_null()) {
            return;
        }
        
        if (!has_value_) {
            max_ = value;
            has_value_ = true;
        } else {
            if (value.compare(max_) > 0) {
                max_ = value;
            }
        }
        
        count_++;
    }
    
    Value result() const override {
        if (!has_value_) {
            Value v;
            v.set_null();
            return v;
        }
        return max_;
    }
    
private:
    Value max_;
    bool has_value_ = false;
};

/**
 * @brief MIN 聚合器
 */
class MinAggregator : public Aggregator {
public:
    void aggregate(const Value &value) override {
        // ========== 跳过 NULL ==========
        if (value.is_null()) {
            return;
        }
        
        if (!has_value_) {
            min_ = value;
            has_value_ = true;
        } else {
            if (value.compare(min_) < 0) {
                min_ = value;
            }
        }
        
        count_++;
    }
    
    Value result() const override {
        if (!has_value_) {
            Value v;
            v.set_null();
            return v;
        }
        return min_;
    }
    
private:
    Value min_;
    bool has_value_ = false;
};
```

---

## 第7章：调试实践

### 🔍 调试断点设置

```cpp
// 断点1：NULL 值插入校验
文件：src/observer/sql/stmt/insert_stmt.cpp
位置：检查 value.is_null() 的分支
目的：验证 NULL 校验逻辑

// 断点2：NULL 位图设置
文件：src/observer/storage/table/table.cpp
位置：Table::make_record 中设置位图的代码
目的：观察 NULL 位图的设置

// 断点3：NULL 比较
文件：src/observer/sql/operator/predicate_physical_operator.cpp
位置：PredicatePhysicalOperator::evaluate 中处理 NULL 的分支
目的：验证三值逻辑

// 断点4：聚合函数
文件：聚合器的 aggregate 方法
目的：验证 NULL 被正确跳过
```

### 🐛 调试步骤

#### 场景1：测试 NULL 插入

```sql
-- 1. 创建表
CREATE TABLE test_null (
    id INT NOT NULL,
    name CHAR(10) NOT NULL,
    age INT NULLABLE
);

-- 2. 插入 NULL 值
INSERT INTO test_null VALUES (1, 'Alice', NULL);

-- 调试观察：
-- 断点1：
--   values[2].is_null() = true
--   field_meta->nullable() = true
--   → 校验通过

-- 断点2：
--   bitmap[0] = 0b00000100  (第3个字段是NULL)
```

#### 场景2：测试非法 NULL 插入

```sql
-- 在不允许 NULL 的字段插入 NULL
INSERT INTO test_null VALUES (2, NULL, 20);

-- 调试观察：
-- 断点1：
--   values[1].is_null() = true
--   field_meta->nullable() = false
--   → 返回 RC::SCHEMA_FIELD_NOT_NULL
```

#### 场景3：测试 IS NULL 查询

```sql
SELECT * FROM test_null WHERE age IS NULL;

-- 调试观察：
-- 断点3：
--   comp = CompOp::IS_NULL
--   left_value.is_null() = true
--   → 条件满足，返回该行
```

### 📊 内存观察

```cpp
// 观察 NULL 位图
// 假设记录数据起始地址为 0x1000
record_data[0] = 0x04  // 二进制 00000100，表示第3个字段是NULL

// 字段偏移量（假设）
// field[0].offset() = 1  (位图后)
// field[1].offset() = 5
// field[2].offset() = 15
```

---

## 第8章：测试用例

### 📝 完整测试用例

```sql
-- ========== 准备工作 ==========
CREATE TABLE test_null (
    id INT NOT NULL,
    name CHAR(10) NOT NULL,
    age INT NULLABLE,
    score FLOAT NULLABLE
);

-- ========== 测试1：正常插入（带 NULL）==========
INSERT INTO test_null VALUES (1, 'Alice', 20, 85.5);
INSERT INTO test_null VALUES (2, 'Bob', NULL, 90.0);
INSERT INTO test_null VALUES (3, 'Carol', 25, NULL);
INSERT INTO test_null VALUES (4, 'David', NULL, NULL);
-- 预期：全部 SUCCESS

-- ========== 测试2：非法 NULL（NOT NULL 字段）==========
INSERT INTO test_null VALUES (5, NULL, 30, 80.0);
-- 预期：FAILURE (name 不允许 NULL)

INSERT INTO test_null VALUES (NULL, 'Eve', 28, 75.0);
-- 预期：FAILURE (id 不允许 NULL)

-- ========== 测试3：查询所有数据 ==========
SELECT * FROM test_null;
-- 预期：显示所有记录，NULL 显示为 "NULL"

-- ========== 测试4：IS NULL 查询 ==========
SELECT * FROM test_null WHERE age IS NULL;
-- 预期：返回 Bob 和 David

SELECT * FROM test_null WHERE score IS NULL;
-- 预期：返回 Carol 和 David

-- ========== 测试5：IS NOT NULL 查询 ==========
SELECT * FROM test_null WHERE age IS NOT NULL;
-- 预期：返回 Alice 和 Carol

-- ========== 测试6：NULL 等值比较（应返回空）==========
SELECT * FROM test_null WHERE age = NULL;
-- 预期：返回空（NULL = NULL 是 UNKNOWN，视为 FALSE）

SELECT * FROM test_null WHERE age <> NULL;
-- 预期：返回空

-- ========== 测试7：NULL 范围比较 ==========
SELECT * FROM test_null WHERE age > 18;
-- 预期：返回 Alice 和 Carol（Bob 和 David 的 age 是 NULL，不满足）

-- ========== 测试8：组合条件 ==========
SELECT * FROM test_null WHERE age IS NULL AND score IS NOT NULL;
-- 预期：返回 Bob

SELECT * FROM test_null WHERE age IS NOT NULL OR score IS NOT NULL;
-- 预期：返回 Alice, Bob, Carol

-- ========== 测试9：COUNT 聚合 ==========
SELECT COUNT(*) FROM test_null;
-- 预期：4（计算所有行）

SELECT COUNT(age) FROM test_null;
-- 预期：2（只计非NULL：Alice, Carol）

SELECT COUNT(score) FROM test_null;
-- 预期：2（只计非NULL：Alice, Bob）

-- ========== 测试10：SUM/AVG 聚合 ==========
SELECT SUM(age) FROM test_null;
-- 预期：45 (20 + 25)

SELECT AVG(age) FROM test_null;
-- 预期：22.5 (45 / 2，不是 45 / 4)

-- ========== 测试11：MAX/MIN 聚合 ==========
SELECT MAX(age) FROM test_null;
-- 预期：25

SELECT MIN(age) FROM test_null;
-- 预期：20

-- ========== 测试12：全 NULL 列的聚合 ==========
-- 假设所有 age 都是 NULL
SELECT AVG(age) FROM (SELECT NULL as age);
-- 预期：NULL（没有非NULL值）
```

### ✅ 测试结果验证表

```
测试用例              | 预期结果    | 实际结果
---------------------|------------|----------
正常插入带NULL        | SUCCESS    | SUCCESS ✓
非法NULL插入          | FAILURE    | FAILURE ✓
IS NULL查询           | 正确返回   | 正确返回 ✓
IS NOT NULL查询       | 正确返回   | 正确返回 ✓
NULL等值比较          | 返回空     | 返回空 ✓
COUNT(*)             | 4          | 4 ✓
COUNT(col)           | 2          | 2 ✓
SUM跳过NULL          | 45         | 45 ✓
AVG跳过NULL          | 22.5       | 22.5 ✓
```

---

## 第9章：总结

### 🎯 核心要点

1. **存储格式**
   - 记录头部增加 NULL 位图
   - 每个字段对应1位
   - 0 = 非NULL，1 = NULL

2. **语法支持**
   - `NULLABLE` 允许 NULL
   - `NOT NULL` 禁止 NULL
   - `IS NULL` / `IS NOT NULL` 判断

3. **三值逻辑**
   - NULL 与任何值比较 = UNKNOWN
   - WHERE 中 UNKNOWN 视为 FALSE
   - 只有 IS NULL/IS NOT NULL 能判断

4. **聚合函数**
   - COUNT(*) 计所有行
   - COUNT(col) 不计 NULL
   - SUM/AVG/MAX/MIN 跳过 NULL

### 📝 实现检查清单

- [ ] Parser：NULL, NULLABLE, IS 关键字
- [ ] 字段元数据：nullable 属性
- [ ] 记录格式：NULL 位图
- [ ] INSERT 校验：NOT NULL 约束
- [ ] 记录创建：设置 NULL 位图
- [ ] 记录读取：检查 NULL 位图
- [ ] 比较逻辑：三值逻辑
- [ ] IS NULL / IS NOT NULL 支持
- [ ] 聚合函数：正确处理 NULL

---

**恭喜！你已经掌握了 NULL 功能的完整实现！** 🎉

