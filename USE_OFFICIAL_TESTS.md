# 使用官方测试用例指南

## 📂 官方测试文件目录

`test/case/` 目录包含官方的测试用例，这些是**最权威、最完整**的测试！

### 目录结构

```
test/case/
├── test/          # 测试 SQL 文件 (.test)
├── result/        # 预期结果文件 (.result)
├── miniob_test.py # 自动化测试脚本
└── README.md      # 说明文档
```

---

## 📋 可用的官方测试文件

### 基础功能测试

| 文件名 | 测试功能 | 对应你的实现 |
|--------|----------|--------------|
| `basic.test` | 基础功能 | ✅ basic |
| `primary-select-meta.result` | 元数据检查 | ✅ select-meta |
| `primary-drop-table.test` | 删除表 | ✅ drop-table |

### 类型功能测试

| 文件名 | 测试功能 | 对应你的实现 |
|--------|----------|--------------|
| `primary-date.test` | 日期类型 | ✅ date |
| `primary-text.test` | 长文本 | ✅ text |
| `primary-null.test` | NULL值 | ✅ null |

### 索引功能测试

| 文件名 | 测试功能 | 对应你的实现 |
|--------|----------|--------------|
| `primary-multi-index.test` | 多列索引 | ✅ multi-index |
| `primary-unique.test` | 唯一索引 | ✅ unique |

### 查询功能测试

| 文件名 | 测试功能 | 对应你的实现 |
|--------|----------|--------------|
| `primary-select-tables.result` | 多表查询 | ✅ select-tables |
| `primary-join-tables.test` | JOIN | ✅ join-tables |
| `primary-order-by.test` | 排序 | ✅ order-by |
| `primary-group-by.test` | 分组 | ✅ group-by |
| `primary-aggregation-func.test` | 聚合函数 | ✅ aggregation-func |

### 高级功能测试

| 文件名 | 测试功能 | 对应你的实现 |
|--------|----------|--------------|
| `primary-simple-sub-query.test` | 简单子查询 | ✅ simple-sub-query |
| `primary-complex-sub-query.test` | 复杂子查询 | ✅ (扩展) |
| `primary-update.test` | 更新 | ✅ update |
| `primary-insert.result` | 多行插入 | ✅ insert |

### 其他测试

| 文件名 | 说明 |
|--------|------|
| `primary-expression.test` | 表达式计算 |
| `vectorized-basic.test` | 向量化执行 |
| `vectorized-aggregation-and-group-by.test` | 向量化聚合 |

---

## 🚀 使用方法

### 方法1：F5 调试 + 手动运行（推荐）⭐

**步骤**：

1. **启动调试**
   ```
   在 VSCode 中按 F5
   ```

2. **选择测试文件**
   ```
   打开 test/case/test/basic.test
   ```

3. **复制 SQL 逐条运行**
   ```sql
   -- 复制这一条，在 DEBUG CONSOLE 中粘贴执行
   create table t_basic(id int, age int, name char, score float);
   
   -- 观察结果，然后继续复制下一条
   insert into t_basic values(1,1, 'a', 1.0);
   ```

4. **对比预期结果**
   ```
   打开 test/case/result/basic.result
   对比你的输出是否与预期一致
   ```

**优点**：
- ✅ 可以设置断点调试
- ✅ 可以单步执行
- ✅ 可以随时暂停检查
- ✅ 适合开发调试

---

### 方法2：CLI 模式批量运行

**步骤**：

```bash
# 1. 启动 CLI 模式
cd build_debug
./bin/observer -f ../etc/observer.ini -P cli

# 2. 复制整个测试文件的内容，粘贴到终端
# 或者使用重定向（如果支持）
./bin/observer -f ../etc/observer.ini -P cli < ../test/case/test/basic.test
```

---

### 方法3：自动化测试脚本（推荐用于回归测试）⭐

**使用 Python 脚本运行**：

```bash
# 1. 启动 observer（另一个终端）
cd build_debug
./bin/observer -f ../etc/observer.ini -s miniob.sock

# 2. 运行测试脚本（新终端）
cd test/case
python3 miniob_test.py --test-cases=basic
```

**运行多个测试**：

```bash
# 运行所有基础测试
python3 miniob_test.py --test-cases=basic,primary-date,primary-text

# 运行所有 primary 测试
python3 miniob_test.py --test-cases=primary-*
```

---

## 📊 推荐的测试顺序

### 第1轮：基础功能（必须通过）

```bash
# 1. basic - 基础功能
test/case/test/basic.test

# 2. drop-table - 删除表
test/case/test/primary-drop-table.test
```

**预期时间**：15-20分钟

---

### 第2轮：类型扩展

```bash
# 1. date - 日期类型
test/case/test/primary-date.test

# 2. text - 长文本
test/case/test/primary-text.test

# 3. null - NULL值
test/case/test/primary-null.test
```

**预期时间**：25-30分钟

---

### 第3轮：索引功能

```bash
# 1. multi-index - 多列索引
test/case/test/primary-multi-index.test

# 2. unique - 唯一索引
test/case/test/primary-unique.test
```

**预期时间**：15-20分钟

---

### 第4轮：查询增强

```bash
# 1. join-tables - JOIN
test/case/test/primary-join-tables.test

# 2. order-by - 排序
test/case/test/primary-order-by.test

# 3. group-by - 分组
test/case/test/primary-group-by.test

# 4. aggregation - 聚合函数
test/case/test/primary-aggregation-func.test
```

**预期时间**：40-50分钟

---

### 第5轮：高级功能

```bash
# 1. simple-sub-query - 简单子查询
test/case/test/primary-simple-sub-query.test

# 2. complex-sub-query - 复杂子查询
test/case/test/primary-complex-sub-query.test

# 3. update - 更新
test/case/test/primary-update.test
```

**预期时间**：30-40分钟

---

## 📝 测试文件格式说明

### .test 文件格式

```sql
-- echo 这是注释，会输出到结果
-- echo initialization

CREATE TABLE test(id int, name char);

-- sort 表示结果需要排序后比较
-- sort SELECT * FROM test;

-- 不带 sort 的查询，结果顺序必须一致
SELECT * FROM test WHERE id = 1;
```

### .result 文件格式

```
这是注释，会输出到结果
initialization
SUCCESS

结果需要排序后比较
SELECT * FROM test;
id | name
1 | Alice
2 | Bob

不带 sort 的查询，结果顺序必须一致
SELECT * FROM test WHERE id = 1;
id | name
1 | Alice
```

---

## 🔍 示例：使用 basic.test

### 1. 查看测试内容

```bash
cat test/case/test/basic.test
```

你会看到：

```sql
-- echo basic insert
create table t_basic(id int, age int, name char, score float);
insert into t_basic values(1,1, 'a', 1.0);
insert into t_basic values(2,2, 'b', 2.0);
...
```

### 2. 查看预期结果

```bash
cat test/case/result/basic.result
```

你会看到：

```
basic insert
SUCCESS
SUCCESS
SUCCESS
...
```

### 3. 在 F5 模式下运行

```
1. 按 F5 启动
2. 在 DEBUG CONSOLE 中复制粘贴 SQL
3. 对比输出与 basic.result 是否一致
```

### 4. 如果结果不一致

- **设置断点**：在相关代码处设置断点
- **单步调试**：F10/F11 单步执行
- **查看变量**：观察中间变量的值
- **查看日志**：检查 observer.log

---

## 🎯 快速验证脚本

创建一个快速验证脚本 `quick_test.sh`：

```bash
#!/bin/bash

# 快速测试脚本
echo "开始快速测试..."

# 启动 observer
cd build_debug
./bin/observer -f ../etc/observer.ini -P cli << EOF
-- 基础测试
create table test(id int, name char(20));
insert into test values(1, 'Alice');
select * from test;

-- DATE 测试
create table t_date(id int, birthday date);
insert into t_date values(1, '2000-01-01');
select * from t_date;

-- JOIN 测试
create table t1(id int, name char(10));
create table t2(id int, value int);
insert into t1 values(1, 'A');
insert into t2 values(1, 100);
select * from t1 inner join t2 on t1.id = t2.id;

exit
EOF

echo "测试完成"
```

---

## 📈 测试结果记录模板

建议创建 `test_results.md` 记录测试结果：

```markdown
# 官方测试结果记录

## 日期：2024-XX-XX

### 基础功能
- [x] basic.test - ✓ 通过
- [x] primary-drop-table.test - ✓ 通过

### 类型功能
- [x] primary-date.test - ✓ 通过
- [ ] primary-text.test - ✗ 失败
  - 失败用例：INSERT 超长文本未正确截断
  - 错误行：第 XX 行
  - 修复计划：...
- [x] primary-null.test - ✓ 通过

### 索引功能
- [ ] primary-multi-index.test - ⚠️ 部分通过
  - 通过：基础多列索引
  - 失败：前缀匹配查询
- [x] primary-unique.test - ✓ 通过

### 查询功能
- [x] primary-join-tables.test - ✓ 通过
- [x] primary-order-by.test - ✓ 通过
- [x] primary-group-by.test - ✓ 通过
- [x] primary-aggregation-func.test - ✓ 通过

### 高级功能
- [x] primary-simple-sub-query.test - ✓ 通过
- [ ] primary-complex-sub-query.test - ✗ 失败
- [x] primary-update.test - ✓ 通过

### 总结
- 通过：12/16
- 失败：2/16
- 部分通过：2/16
- 总体进度：75%
```

---

## 🐛 常见问题

### 问题1：测试文件中的 `--echo` 和 `--sort` 是什么？

**答**：
- `--echo`：注释会输出到结果中
- `--sort`：查询结果需要排序后再比较（因为某些查询结果顺序可能不确定）

### 问题2：如何只运行某一个测试用例？

**答**：
```bash
# 方法1：手动复制
打开 .test 文件，复制想要的 SQL，粘贴到 CLI 模式运行

# 方法2：使用 Python 脚本
python3 miniob_test.py --test-cases=basic
```

### 问题3：测试结果与 .result 文件不一致怎么办？

**答**：
1. 检查是否是因为输出格式不同（空格、换行等）
2. 检查结果的逻辑是否正确（可能格式不同但逻辑正确）
3. 如果逻辑错误，使用 F5 调试模式找到问题
4. 查看 MySQL 8.0 的结果作为参考

### 问题4：.test 文件太长，如何快速定位问题？

**答**：
1. 先运行前几条 SQL，逐步增加
2. 遇到失败的，记录下来
3. 单独调试失败的 SQL

---

## ✅ 测试检查清单

使用官方测试文件的检查清单：

### 基础功能
- [ ] basic.test - 所有测试通过
- [ ] primary-drop-table.test - 表删除功能正常

### 类型功能
- [ ] primary-date.test - 日期类型完全支持
- [ ] primary-text.test - 长文本正确处理
- [ ] primary-null.test - NULL值处理正确

### 索引功能
- [ ] primary-multi-index.test - 多列索引功能正常
- [ ] primary-unique.test - 唯一约束正确

### 查询功能
- [ ] primary-join-tables.test - JOIN功能完整
- [ ] primary-order-by.test - 排序正确
- [ ] primary-group-by.test - 分组正确
- [ ] primary-aggregation-func.test - 聚合函数正确

### 高级功能
- [ ] primary-simple-sub-query.test - 简单子查询正确
- [ ] primary-complex-sub-query.test - 复杂子查询正确
- [ ] primary-update.test - 更新功能正常

---

## 🎉 优势

使用官方测试文件的优势：

1. ✅ **标准化**：这是官方测试，更接近实际考核
2. ✅ **完整性**：覆盖了各种边界情况
3. ✅ **可验证**：有 .result 文件可以对比
4. ✅ **可回归**：修复问题后可以重新运行
5. ✅ **可自动化**：可以使用 Python 脚本批量运行

---

## 💡 建议

1. **先用我创建的测试文件快速验证**（test_plan_stage*.sql）
   - 快速了解功能是否基本可用
   
2. **再用官方测试文件详细测试**（test/case/test/*.test）
   - 确保所有边界情况都处理正确
   
3. **最后用 Python 脚本做回归测试**
   - 确保修复一个问题不会破坏其他功能

---

## 🚀 立即开始

```bash
# 1. 编译项目
bash build.sh debug

# 2. 按 F5 启动调试

# 3. 打开第一个官方测试文件
cat test/case/test/basic.test

# 4. 复制 SQL 到 DEBUG CONSOLE 运行

# 5. 对比结果
cat test/case/result/basic.result
```

祝测试顺利！🎊

