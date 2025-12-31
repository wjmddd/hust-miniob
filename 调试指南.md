# MiniOB 功能调试指南

## 📋 目录

- [快速开始](#快速开始)
- [测试阶段说明](#测试阶段说明)
- [调试方法](#调试方法)
- [常见问题排查](#常见问题排查)
- [性能测试](#性能测试)

---

## 🚀 快速开始

### 1. 编译项目

```bash
# 编译 Debug 版本（包含调试信息）
bash build.sh debug

# 或者编译 Release 版本（性能测试）
bash build.sh release
```

### 2. 选择测试方式

#### **方式 A：F5 调试模式（推荐用于开发调试）**

1. 在 VSCode 中打开项目
2. 按 `F5` 启动调试
3. 在 `DEBUG CONSOLE` 中会看到 `miniob >` 提示符
4. 复制测试文件中的 SQL，粘贴执行
5. 可以设置断点，单步调试

**优点**：
- ✅ 可以设置断点
- ✅ 可以查看变量值
- ✅ 可以单步执行
- ✅ 适合深入理解代码执行流程

#### **方式 B：CLI 命令行模式**

```bash
cd build_debug
./bin/observer -f ../etc/observer.ini -P cli
```

然后直接在终端输入 SQL 命令。

**优点**：
- ✅ 启动快速
- ✅ 适合快速验证功能
- ✅ 输出清晰

#### **方式 C：客户端-服务端模式**

终端1（服务端）：
```bash
cd build_debug
./bin/observer -f ../etc/observer.ini -s miniob.sock
```

终端2（客户端）：
```bash
cd build_debug
./bin/obclient -s miniob.sock
```

**优点**：
- ✅ 模拟真实使用场景
- ✅ 可以测试网络通信
- ✅ 适合压力测试

---

## 📊 测试阶段说明

### 阶段 1：基础功能测试 [关键] ⭐⭐⭐

**文件**：`test_plan_stage1_basic.sql`

**测试功能**：
- ✓ basic：创建表、插入、查询、索引
- ✓ select-meta：元数据检查
- ✓ drop-table：删除表

**预计时间**：15-20分钟

**调试重点**：
1. 表的创建和删除是否正常
2. 索引的创建和使用是否正确
3. 错误的SQL是否能正确返回FAILURE
4. 删除表后，相关索引是否也被删除

**运行方法**：
```bash
# F5 模式
按 F5 后，在 DEBUG CONSOLE 中逐条运行 test_plan_stage1_basic.sql 中的 SQL

# CLI 模式
./bin/observer -f ../etc/observer.ini -P cli
# 然后复制粘贴 SQL
```

**常见问题**：
- 如果基础功能有问题，后续测试无法进行
- 建议先修复这个阶段的所有问题

---

### 阶段 2：类型扩展测试 [关键] ⭐⭐⭐

**文件**：`test_plan_stage2_types.sql`

**测试功能**：
- ✓ date：日期类型
- ✓ text：长文本（4096字节）
- ✓ null：NULL值支持

**预计时间**：20-30分钟

**调试重点**：
1. **DATE**：
   - 日期的解析和存储
   - 闰年处理（2024-02-29）
   - 非法日期拒绝（2023-02-29）
   - 日期比较和索引
   
2. **TEXT**：
   - 超过4096字节的截断
   - TEXT字段的查询
   - 变长字段的record管理
   
3. **NULL**：
   - NULL的存储和表示
   - NULL的比较规则（NULL与任何值比较都是FALSE）
   - 聚合函数中NULL的处理

**断点建议**：
```cpp
// DATE 类型处理
src/observer/common/type/date_type.cpp

// TEXT 存储
src/observer/storage/record/record_manager.cpp

// NULL 值处理
src/observer/sql/executor/execute_stage.cpp
```

---

### 阶段 3：索引功能测试 ⭐⭐

**文件**：`test_plan_stage3_index.sql`

**测试功能**：
- ✓ multi-index：多列索引
- ✓ unique：唯一索引

**预计时间**：15-20分钟

**调试重点**：
1. **多列索引**：
   - 前缀匹配原则
   - 索引的排序规则（先第一列，再第二列）
   - 查询优化器是否能选择正确的索引
   
2. **唯一索引**：
   - 插入重复值是否正确拒绝
   - 唯一约束的性能
   - NULL值在唯一索引中的处理

**断点建议**：
```cpp
// B+树索引
src/observer/storage/index/bplus_tree.cpp
src/observer/storage/index/bplus_tree_index.cpp

// 查询优化器
src/observer/sql/optimizer/physical_plan_generator.cpp
```

---

### 阶段 4：查询增强测试 ⭐⭐⭐

**文件**：`test_plan_stage4_query.sql`

**测试功能**：
- ✓ select-tables：笛卡尔积
- ✓ join-tables：INNER JOIN
- ✓ order-by：排序
- ✓ group-by：分组
- ✓ aggregation-func：聚合函数

**预计时间**：30-40分钟

**调试重点**：
1. **多表查询**：
   - 笛卡尔积的结果数量
   - JOIN的ON条件处理
   - 多表JOIN的嵌套
   
2. **排序**：
   - ASC/DESC
   - 多列排序
   - 字符串排序
   
3. **分组和聚合**：
   - GROUP BY的正确性
   - COUNT/MAX/MIN/AVG的计算
   - NULL值在聚合中的处理

**断点建议**：
```cpp
// JOIN 操作
src/observer/sql/operator/join_physical_operator.cpp

// 排序操作
src/observer/sql/operator/order_by_physical_operator.cpp

// 分组操作
src/observer/sql/operator/group_by_physical_operator.cpp

// 聚合函数
src/observer/sql/expr/aggregator.cpp
```

---

### 阶段 5：高级功能测试 ⭐⭐

**文件**：`test_plan_stage5_advanced.sql`

**测试功能**：
- ✓ simple-sub-query：子查询
- ✓ update：更新
- ✓ insert：多行插入

**预计时间**：20-30分钟

**调试重点**：
1. **子查询**：
   - IN/NOT IN
   - 比较运算符子查询
   - 子查询结果的正确性
   
2. **UPDATE**：
   - 带条件的更新
   - 不带条件的全表更新
   - 更新后索引的维护
   
3. **多行插入**：
   - 原子性（全部成功或全部失败）
   - 遇到唯一约束冲突的处理

**断点建议**：
```cpp
// 子查询
src/observer/sql/optimizer/predicate_rewrite.cpp

// UPDATE
src/observer/sql/operator/update_physical_operator.cpp

// INSERT
src/observer/sql/operator/insert_physical_operator.cpp
```

---

### 回归测试 ⭐⭐⭐

**文件**：`test_plan_regression.sql`

**测试功能**：综合测试，验证功能组合

**预计时间**：20-30分钟

**调试重点**：
- 多个功能组合使用时是否正常
- 性能是否可接受
- 是否有内存泄漏

---

## 🔍 调试方法

### 方法1：逐条执行 + 验证结果

```sql
-- 1. 执行 SQL
CREATE TABLE test (id INT, name CHAR(20));

-- 2. 验证表是否创建成功
-- 可以通过再次查询或插入数据验证

-- 3. 插入测试数据
INSERT INTO test VALUES (1, 'Alice');

-- 4. 验证查询
SELECT * FROM test;
-- 预期结果：
-- id | name
-- 1  | Alice

-- 5. 清理
DROP TABLE test;
```

### 方法2：设置断点调试

**场景：调试 SELECT 语句的执行**

1. 在关键文件设置断点：
   ```
   src/observer/sql/parser/parse_stage.cpp:45  (SQL解析)
   src/observer/sql/optimizer/optimize_stage.cpp:30  (优化)
   src/observer/sql/executor/execute_stage.cpp:50  (执行)
   ```

2. 按 F5 启动调试

3. 输入 SQL：
   ```sql
   SELECT * FROM students WHERE age > 20;
   ```

4. 程序会在断点处停下，可以：
   - 查看语法树结构
   - 查看优化后的执行计划
   - 单步执行，观察数据流

### 方法3：日志调试

修改 `etc/observer.ini`：

```ini
LOG_FILE_LEVEL=5    # 最详细的日志
LOG_CONSOLE_LEVEL=3 # 控制台显示INFO级别
```

然后查看日志文件 `build_debug/observer.log`

### 方法4：使用 GDB（Linux）

```bash
gdb ./build_debug/bin/observer
(gdb) run -f ../etc/observer.ini -P cli
(gdb) break execute_stage.cpp:50
(gdb) continue
# 输入 SQL
(gdb) print *stmt
(gdb) step
```

---

## 🐛 常见问题排查

### 问题1：段错误（Segmentation Fault）

**排查步骤**：
1. 使用 GDB 定位崩溃位置
2. 检查指针是否为空
3. 检查数组越界
4. 检查内存泄漏

**常见原因**：
- 空指针解引用
- 数组越界访问
- 使用已释放的内存

### 问题2：查询结果不正确

**排查步骤**：
1. 打印中间结果
2. 检查条件判断逻辑
3. 验证类型转换
4. 检查NULL值处理

### 问题3：索引未被使用

**排查步骤**：
1. 检查查询条件是否包含索引列
2. 多列索引要遵循前缀原则
3. 查看优化器的执行计划
4. 检查索引统计信息

### 问题4：多表JOIN结果错误

**排查步骤**：
1. 先验证笛卡尔积是否正确
2. 检查ON条件的处理
3. 验证JOIN算法的实现
4. 检查临时表的生成

---

## 📈 性能测试

### 测试1：大数据量插入

```sql
-- 创建测试表
CREATE TABLE perf_test (id INT, value INT);

-- 插入大量数据（可以写脚本生成）
INSERT INTO perf_test VALUES (1, 100), (2, 200), ..., (10000, 1000000);

-- 测试查询性能
SELECT COUNT(*) FROM perf_test;
SELECT * FROM perf_test WHERE id > 5000;
```

### 测试2：索引性能对比

```sql
-- 无索引查询
SELECT * FROM perf_test WHERE id = 5000;
-- 记录耗时

-- 创建索引
CREATE INDEX idx_id ON perf_test(id);

-- 有索引查询
SELECT * FROM perf_test WHERE id = 5000;
-- 对比耗时
```

### 测试3：JOIN性能

```sql
-- 小表 JOIN 小表
-- 中表 JOIN 中表
-- 大表 JOIN 大表
-- 观察性能差异
```

---

## ✅ 测试检查清单

### 基础功能
- [ ] 表的创建和删除
- [ ] 数据的插入和查询
- [ ] 索引的创建和使用
- [ ] 错误SQL的拒绝

### 类型功能
- [ ] DATE 类型的解析和存储
- [ ] DATE 闰年和非法日期处理
- [ ] TEXT 超长截断
- [ ] NULL 值的存储和比较

### 索引功能
- [ ] 多列索引的前缀匹配
- [ ] 唯一索引的约束检查
- [ ] 索引的性能提升

### 查询功能
- [ ] 笛卡尔积正确性
- [ ] INNER JOIN 正确性
- [ ] ORDER BY 排序正确
- [ ] GROUP BY 分组正确
- [ ] 聚合函数计算正确

### 高级功能
- [ ] 子查询 IN/NOT IN
- [ ] 子查询比较运算
- [ ] UPDATE 功能
- [ ] 多行 INSERT 的原子性

### 综合测试
- [ ] 功能组合使用正常
- [ ] 无内存泄漏
- [ ] 性能可接受
- [ ] 边界情况处理正确

---

## 📝 测试记录模板

建议创建一个测试记录文件 `test_results.md`：

```markdown
# 测试记录

## 日期：2024-XX-XX

### 阶段1：基础功能
- [x] 表创建 - ✓ 通过
- [x] 数据插入 - ✓ 通过
- [ ] 索引创建 - ✗ 失败（原因：...）
- [x] 表删除 - ✓ 通过

### 阶段2：类型扩展
...

## 发现的问题
1. 问题描述：...
   - 复现步骤：...
   - 错误信息：...
   - 修复方案：...

2. ...
```

---

## 🎯 总结

1. **按顺序测试**：从基础到高级，确保基础功能稳定
2. **充分验证**：每个功能都要测试正常情况和边界情况
3. **记录问题**：发现问题及时记录，方便后续修复
4. **性能关注**：不仅要功能正确，还要性能可接受
5. **回归测试**：修复问题后，重新运行所有测试

祝调试顺利！🚀

