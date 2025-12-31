# 🎯 完整测试策略

## 两套测试方案对比

### 方案A：我创建的测试文件（快速验证）

| 文件 | 优点 | 适用场景 |
|------|------|----------|
| `test_plan_stage*.sql` | • 简洁明了<br>• 分阶段<br>• 快速验证 | • 初步验证功能<br>• 开发过程中快速测试<br>• 理解功能逻辑 |

### 方案B：官方测试文件（标准测试）⭐

| 文件 | 优点 | 适用场景 |
|------|------|----------|
| `test/case/test/*.test` | • 官方标准<br>• 全面完整<br>• 有预期结果 | • 完整功能测试<br>• 回归测试<br>• 最终验证 |

---

## 🚀 推荐的三阶段测试策略

### 📍 第一阶段：快速验证（使用我的测试文件）

**目的**：快速了解功能是否基本可用

**时间**：30-60分钟

**方法**：
```bash
# 1. 按 F5 启动调试
# 2. 逐个运行我创建的测试文件

test_plan_stage1_basic.sql      # 基础功能
test_plan_stage2_types.sql      # 类型扩展
test_plan_stage3_index.sql      # 索引功能
test_plan_stage4_query.sql      # 查询增强
test_plan_stage5_advanced.sql   # 高级功能
```

**结果判断**：
- ✅ 如果大部分通过 → 进入第二阶段
- ❌ 如果很多失败 → 先修复基础问题

---

### 📍 第二阶段：详细测试（使用官方测试文件）⭐

**目的**：确保功能完全正确，处理所有边界情况

**时间**：2-3小时

**方法**：

#### 步骤1：按功能分类测试

**基础功能**（必须通过）：
```bash
test/case/test/basic.test
test/case/test/primary-drop-table.test
```

**类型功能**：
```bash
test/case/test/primary-date.test       # DATE 类型
test/case/test/primary-text.test       # TEXT 类型  
test/case/test/primary-null.test       # NULL 支持
```

**索引功能**：
```bash
test/case/test/primary-multi-index.test   # 多列索引
test/case/test/primary-unique.test        # 唯一索引
```

**查询功能**：
```bash
test/case/test/primary-join-tables.test      # JOIN
test/case/test/primary-order-by.test         # ORDER BY
test/case/test/primary-group-by.test         # GROUP BY
test/case/test/primary-aggregation-func.test # 聚合函数
```

**高级功能**：
```bash
test/case/test/primary-simple-sub-query.test   # 简单子查询
test/case/test/primary-complex-sub-query.test  # 复杂子查询
test/case/test/primary-update.test             # UPDATE
```

#### 步骤2：运行方式

**推荐方式：F5 调试 + 手动运行**

```
1. 打开 test/case/test/basic.test
2. 按 F5 启动调试
3. 逐条复制 SQL 到 DEBUG CONSOLE
4. 观察结果，与 test/case/result/basic.result 对比
5. 如果不一致，设置断点调试
```

**批量测试方式：Python 脚本**

```bash
# 终端1：启动服务
cd build_debug
./bin/observer -f ../etc/observer.ini -s miniob.sock

# 终端2：运行测试
cd test/case
python3 miniob_test.py --test-cases=basic
```

---

### 📍 第三阶段：回归测试

**目的**：确保修复问题后没有破坏其他功能

**时间**：1-2小时

**方法**：

```bash
# 方法1：运行所有官方测试
cd test/case
python3 miniob_test.py

# 方法2：运行关键测试
python3 miniob_test.py --test-cases=basic,primary-date,primary-join-tables,primary-group-by

# 方法3：手动运行我的回归测试
# F5 模式，运行 test_plan_regression.sql
```

---

## 📊 完整测试流程图

```
开始
  ↓
编译项目 (bash build.sh debug)
  ↓
┌─────────────────────────────────┐
│ 第一阶段：快速验证 (30-60分钟)  │
│ 使用：test_plan_stage*.sql     │
└─────────────────────────────────┘
  ↓
所有功能基本可用？
  ├─ 否 → 修复基础问题 → 重新测试第一阶段
  ↓
  是
  ↓
┌─────────────────────────────────┐
│ 第二阶段：详细测试 (2-3小时)    │
│ 使用：test/case/test/*.test    │
│ 逐个功能深入测试                │
└─────────────────────────────────┘
  ↓
记录失败的测试用例
  ↓
修复问题
  ↓
重新运行失败的测试
  ↓
┌─────────────────────────────────┐
│ 第三阶段：回归测试 (1-2小时)    │
│ 运行所有测试                    │
│ 确保没有破坏已有功能            │
└─────────────────────────────────┘
  ↓
所有测试通过？
  ├─ 否 → 返回第二阶段
  ↓
  是
  ↓
完成！🎉
```

---

## 🎯 具体操作示例

### 示例1：测试 DATE 功能

#### 第一轮：快速验证

```bash
# 使用 test_plan_stage2_types.sql
# 按 F5，在 DEBUG CONSOLE 中运行：

CREATE TABLE t_date (id INT, birthday DATE);
INSERT INTO t_date VALUES (1, '2000-01-01');
INSERT INTO t_date VALUES (2, '2024-02-29');  -- 闰年
SELECT * FROM t_date WHERE birthday > '1999-01-01';
```

**结果**：基本功能可用 ✅

#### 第二轮：详细测试

```bash
# 使用 test/case/test/primary-date.test
# 包含更多边界情况：

1. 打开 test/case/test/primary-date.test
2. 复制所有 SQL 到 DEBUG CONSOLE
3. 对比结果与 test/case/result/primary-date.result

# 发现问题：
- ✅ 正常日期：通过
- ✅ 闰年判断：通过
- ❌ 非法日期拒绝：失败（2017-02-29 应该返回 FAILURE）
```

#### 修复问题

```cpp
// 在 src/observer/common/type/date_type.cpp 中
// 设置断点，调试闰年判断逻辑
// 修复后重新测试
```

#### 第三轮：回归测试

```bash
# 重新运行所有与 DATE 相关的测试
# 确保修复没有破坏其他功能
```

---

### 示例2：测试 JOIN 功能

#### 第一轮：快速验证

```bash
# 使用 test_plan_stage4_query.sql
CREATE TABLE t1 (id INT, name CHAR(20));
CREATE TABLE t2 (id INT, value INT);
INSERT INTO t1 VALUES (1, 'Alice'), (2, 'Bob');
INSERT INTO t2 VALUES (1, 100), (2, 200);
SELECT * FROM t1 INNER JOIN t2 ON t1.id = t2.id;
```

**结果**：基本 JOIN 可用 ✅

#### 第二轮：详细测试

```bash
# 使用 test/case/test/primary-join-tables.test
# 包含：
- 基础 JOIN
- 多表 JOIN (3个表)
- 带 WHERE 的 JOIN
- 空表 JOIN
- 大数据量 JOIN

# 发现问题：
- ✅ 2表 JOIN：通过
- ❌ 3表 JOIN：失败
- ✅ 带条件 JOIN：通过
- ❌ 空表 JOIN：崩溃
```

#### 修复问题

```cpp
// 设置断点在：
src/observer/sql/operator/join_physical_operator.cpp

// 调试：
- 3表 JOIN 的嵌套逻辑
- 空表的边界情况处理
```

---

## 📝 测试记录模板

建议创建 `my_test_log.md`：

```markdown
# 测试日志

## 第一阶段：快速验证

### 日期：2024-XX-XX

#### test_plan_stage1_basic.sql
- [x] 表创建 ✅
- [x] 数据插入 ✅
- [x] 查询 ✅
- [x] 索引 ✅
- [x] 删除表 ✅

#### test_plan_stage2_types.sql
- [x] DATE ✅
- [x] TEXT ✅
- [ ] NULL ❌ - AVG 函数未正确处理 NULL

**第一阶段结论**：大部分功能可用，进入第二阶段

---

## 第二阶段：详细测试

### basic.test
- 状态：✅ 完全通过
- 耗时：15分钟

### primary-date.test
- 状态：⚠️ 部分通过
- 失败用例：非法日期应该返回 FAILURE
- 失败行：第31-34行
- 问题描述：2017-02-29 (非闰年) 未被拒绝
- 修复计划：增加闰年判断逻辑

### primary-join-tables.test
- 状态：❌ 失败
- 失败用例：3表 JOIN
- 问题描述：结果数量不正确
- 修复计划：检查嵌套 JOIN 逻辑

---

## 第三阶段：回归测试

### 修复后重测
- [x] primary-date.test ✅
- [x] primary-join-tables.test ✅
- [x] basic.test ✅ (确保没有破坏)

### 全量测试
```bash
python3 miniob_test.py
```

结果：16/16 通过 ✅

---

## 总结

- 第一阶段耗时：1小时
- 第二阶段耗时：3小时
- 修复问题：5个
- 第三阶段耗时：1.5小时
- 总耗时：5.5小时
- 最终结果：所有测试通过 🎉
```

---

## 🔧 调试技巧

### 技巧1：逐步缩小问题范围

```
官方测试失败
  ↓
确定失败的具体 SQL
  ↓
在我的简化测试中复现
  ↓
设置断点调试
  ↓
修复问题
  ↓
重新运行官方测试
```

### 技巧2：对比 MySQL 结果

```bash
# 在 MySQL 中运行相同的 SQL
mysql> CREATE TABLE test(id INT, name CHAR(20));
mysql> INSERT INTO test VALUES (1, 'Alice');
mysql> SELECT * FROM test;

# 对比 MiniOB 的结果
# 如果不一致，说明逻辑有问题
```

### 技巧3：使用断点调试链

```
SQL 解析 → 语义分析 → 优化 → 执行

在每个阶段设置断点：
1. parse_stage.cpp - 检查语法树
2. resolve_stage.cpp - 检查语义分析
3. optimize_stage.cpp - 检查执行计划
4. execute_stage.cpp - 检查执行过程
```

---

## 📈 进度跟踪

### 测试完成度

| 分类 | 快速验证 | 详细测试 | 回归测试 |
|------|---------|---------|---------|
| 基础功能 | ☐ | ☐ | ☐ |
| 类型扩展 | ☐ | ☐ | ☐ |
| 索引功能 | ☐ | ☐ | ☐ |
| 查询增强 | ☐ | ☐ | ☐ |
| 高级功能 | ☐ | ☐ | ☐ |

### 问题跟踪

| 功能 | 问题描述 | 状态 | 修复时间 |
|------|---------|------|---------|
| DATE | 闰年判断错误 | ✅ 已修复 | 2024-XX-XX |
| JOIN | 3表JOIN失败 | 🔧 修复中 | - |
| ... | ... | ... | ... |

---

## 🎯 成功标准

### 第一阶段成功标准
- [ ] 所有 test_plan_stage*.sql 中的基本用例都能运行
- [ ] 无崩溃或段错误
- [ ] 结果逻辑基本正确

### 第二阶段成功标准
- [ ] 所有官方 .test 文件通过
- [ ] 结果与 .result 文件完全一致
- [ ] 边界情况都正确处理

### 第三阶段成功标准
- [ ] 批量运行所有测试都通过
- [ ] 无性能严重下降
- [ ] 无内存泄漏

---

## 💡 最佳实践

1. **先简后繁**：先用简单测试快速验证，再用复杂测试详细检查
2. **及时记录**：发现问题立即记录，避免遗忘
3. **逐个击破**：不要同时修复多个问题，容易混乱
4. **回归验证**：每次修复后都要运行回归测试
5. **性能关注**：功能正确的同时也要注意性能

---

## 🚀 立即开始

**今天就开始测试！**

```bash
# 第一步：编译
bash build.sh debug

# 第二步：快速验证（1小时）
按 F5，运行 test_plan_stage1_basic.sql

# 第三步：详细测试（今天先测1-2个）
运行 test/case/test/basic.test

# 第四步：记录结果
创建 my_test_log.md，记录测试结果
```

**循序渐进，稳扎稳打！** 🎊

