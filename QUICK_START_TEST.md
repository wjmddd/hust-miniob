# 🚀 快速开始测试

## 1️⃣ 准备工作（5分钟）

```bash
# 编译项目
bash build.sh debug

# 检查编译结果
ls build_debug/bin/observer
ls build_debug/bin/obclient
```

---

## 2️⃣ 选择测试方式

### ⭐ 推荐：F5 调试模式

```
1. 在 VSCode 中按 F5
2. 在 DEBUG CONSOLE 中看到 "miniob >" 提示符
3. 复制粘贴测试 SQL，回车执行
4. 观察输出结果
```

**优势**：可以设置断点，单步调试，适合深入理解代码

---

## 3️⃣ 按顺序测试（重要！）

### 📋 测试顺序

```
阶段1: 基础功能 ⭐⭐⭐ [必须通过]
  ↓
阶段2: 类型扩展 ⭐⭐⭐ [必须通过]
  ↓
阶段3: 索引功能 ⭐⭐
  ↓
阶段4: 查询增强 ⭐⭐⭐
  ↓
阶段5: 高级功能 ⭐⭐
  ↓
回归测试 ⭐⭐⭐
```

---

## 4️⃣ 开始测试

### 阶段1：基础功能测试（15分钟）

**文件**：`test_plan_stage1_basic.sql`

**快速测试**：

```sql
-- 1. 创建表
CREATE TABLE test (id INT, name CHAR(20));

-- 2. 插入数据
INSERT INTO test VALUES (1, 'Alice');

-- 3. 查询
SELECT * FROM test;
-- 预期：显示 id=1, name=Alice

-- 4. 创建索引
CREATE INDEX idx_id ON test(id);

-- 5. 使用索引查询
SELECT * FROM test WHERE id = 1;
-- 预期：显示 id=1, name=Alice

-- 6. 删除表
DROP TABLE test;

-- 7. 验证表已删除
SELECT * FROM test;
-- 预期：FAILURE
```

**如果通过**：✅ 继续阶段2
**如果失败**：❌ 停止，先修复问题

---

### 阶段2：类型扩展测试（20分钟）

**文件**：`test_plan_stage2_types.sql`

**快速测试**：

```sql
-- 测试 DATE
CREATE TABLE t_date (id INT, birthday DATE);
INSERT INTO t_date VALUES (1, '2000-01-01');
SELECT * FROM t_date WHERE birthday > '1999-01-01';

-- 测试 TEXT
CREATE TABLE t_text (id INT, content TEXT);
INSERT INTO t_text VALUES (1, 'This is a long text content for testing');
SELECT * FROM t_text;

-- 测试 NULL
CREATE TABLE t_null (id INT NOT NULL, name CHAR(20) NULLABLE);
INSERT INTO t_null VALUES (1, 'Alice');
INSERT INTO t_null VALUES (2, NULL);
SELECT * FROM t_null;

-- 清理
DROP TABLE t_date;
DROP TABLE t_text;
DROP TABLE t_null;
```

---

### 阶段3：索引功能测试（15分钟）

**文件**：`test_plan_stage3_index.sql`

**快速测试**：

```sql
-- 测试多列索引
CREATE TABLE t_multi (id INT, age INT, name CHAR(20));
CREATE INDEX idx_age_name ON t_multi(age, name);
INSERT INTO t_multi VALUES (1, 20, 'Alice'), (2, 25, 'Bob');
SELECT * FROM t_multi WHERE age = 20;  -- 应该使用索引

-- 测试唯一索引
CREATE TABLE t_unique (id INT, email CHAR(50));
CREATE UNIQUE INDEX idx_email ON t_unique(email);
INSERT INTO t_unique VALUES (1, 'alice@test.com');
INSERT INTO t_unique VALUES (2, 'bob@test.com');
INSERT INTO t_unique VALUES (3, 'alice@test.com');  -- 应该失败

-- 清理
DROP TABLE t_multi;
DROP TABLE t_unique;
```

---

### 阶段4：查询增强测试（30分钟）

**文件**：`test_plan_stage4_query.sql`

**快速测试**：

```sql
-- 准备数据
CREATE TABLE students (id INT, name CHAR(20), class_id INT);
CREATE TABLE classes (id INT, class_name CHAR(30));
INSERT INTO students VALUES (1, 'Alice', 1), (2, 'Bob', 2);
INSERT INTO classes VALUES (1, 'Class A'), (2, 'Class B');

-- 测试笛卡尔积
SELECT * FROM students, classes;  -- 应该返回 2*2=4 行

-- 测试 JOIN
SELECT students.name, classes.class_name 
FROM students 
INNER JOIN classes ON students.class_id = classes.id;

-- 测试 ORDER BY
SELECT * FROM students ORDER BY name;

-- 测试 GROUP BY
SELECT class_id, COUNT(*) FROM students GROUP BY class_id;

-- 测试聚合函数
SELECT COUNT(*), AVG(id) FROM students;

-- 清理
DROP TABLE students;
DROP TABLE classes;
```

---

### 阶段5：高级功能测试（20分钟）

**文件**：`test_plan_stage5_advanced.sql`

**快速测试**：

```sql
-- 准备数据
CREATE TABLE emp (id INT, name CHAR(20), age INT);
INSERT INTO emp VALUES (1, 'Alice', 30), (2, 'Bob', 35), (3, 'Charlie', 28);

-- 测试多行插入
CREATE TABLE test_insert (id INT, value INT);
INSERT INTO test_insert VALUES (1, 100), (2, 200), (3, 300);
SELECT COUNT(*) FROM test_insert;  -- 应该是3

-- 测试 UPDATE
UPDATE emp SET age = 31 WHERE name = 'Alice';
SELECT * FROM emp WHERE name = 'Alice';  -- age 应该是31

-- 测试子查询
SELECT * FROM emp WHERE age > (SELECT AVG(age) FROM emp);

-- 清理
DROP TABLE emp;
DROP TABLE test_insert;
```

---

## 5️⃣ 调试技巧

### 🔍 设置断点

在以下文件设置断点，观察执行流程：

```cpp
// SQL 解析
src/observer/sql/parser/parse_stage.cpp:45

// 查询优化
src/observer/sql/optimizer/optimize_stage.cpp:30

// 执行阶段
src/observer/sql/executor/execute_stage.cpp:50

// 存储引擎
src/observer/storage/table/table.cpp:100
```

### 📝 查看日志

```bash
# 查看详细日志
tail -f build_debug/observer.log
```

### 🐛 常见问题

**问题1：表已存在**
```sql
-- 先删除再创建
DROP TABLE test;
CREATE TABLE test (id INT);
```

**问题2：段错误**
```bash
# 使用 GDB 调试
gdb ./build_debug/bin/observer
(gdb) run -f ../etc/observer.ini -P cli
# 崩溃后输入 bt 查看堆栈
(gdb) bt
```

**问题3：结果不符合预期**
- 检查 WHERE 条件
- 检查数据类型
- 检查 NULL 值处理

---

## 6️⃣ 检查清单

完成每个阶段后，勾选：

- [ ] 阶段1: 基础功能 - 表创建、插入、查询、索引
- [ ] 阶段2: 类型扩展 - DATE、TEXT、NULL
- [ ] 阶段3: 索引功能 - 多列索引、唯一索引
- [ ] 阶段4: 查询增强 - JOIN、ORDER BY、GROUP BY、聚合
- [ ] 阶段5: 高级功能 - 子查询、UPDATE、多行INSERT
- [ ] 回归测试: 功能组合测试

---

## 7️⃣ 测试命令速查

```bash
# 编译
bash build.sh debug

# F5 调试（在 VSCode 中）
按 F5 -> 在 DEBUG CONSOLE 输入 SQL

# CLI 模式
cd build_debug
./bin/observer -f ../etc/observer.ini -P cli

# 客户端模式
# 终端1
./bin/observer -f ../etc/observer.ini -s miniob.sock
# 终端2
./bin/obclient -s miniob.sock

# 运行单元测试
cd build_debug
ctest

# 查看日志
tail -f build_debug/observer.log
```

---

## 8️⃣ 预期时间

| 阶段 | 预计时间 | 难度 |
|------|---------|------|
| 阶段1: 基础功能 | 15-20分钟 | ⭐⭐ |
| 阶段2: 类型扩展 | 20-30分钟 | ⭐⭐⭐ |
| 阶段3: 索引功能 | 15-20分钟 | ⭐⭐ |
| 阶段4: 查询增强 | 30-40分钟 | ⭐⭐⭐ |
| 阶段5: 高级功能 | 20-30分钟 | ⭐⭐ |
| 回归测试 | 20-30分钟 | ⭐⭐⭐ |
| **总计** | **2-3小时** | |

---

## 💡 提示

1. **不要跳过阶段**：每个阶段都有依赖关系
2. **遇到问题先记录**：记录错误信息和复现步骤
3. **使用断点调试**：深入理解代码执行流程
4. **保存测试结果**：方便回归测试
5. **性能也很重要**：不仅要功能正确，还要性能合理

---

## 📞 需要帮助？

- 查看详细文档：`DEBUG_GUIDE.md`
- 查看测试文件：`test_plan_stage*.sql`
- 查看代码注释：各模块的 `.h` 和 `.cpp` 文件

祝测试顺利！🎉

