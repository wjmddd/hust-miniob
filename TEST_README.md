# MiniOB 调试测试方案

## 📚 文件说明

本目录包含一套完整的 MiniOB 功能调试测试方案，帮助系统化地测试所有实现的功能。

### 测试文件列表

| 文件名 | 说明 | 测试功能 | 优先级 |
|--------|------|----------|--------|
| `test_plan_stage1_basic.sql` | 阶段1：基础功能测试 | basic, select-meta, drop-table | ⭐⭐⭐ 关键 |
| `test_plan_stage2_types.sql` | 阶段2：类型扩展测试 | date, text, null | ⭐⭐⭐ 关键 |
| `test_plan_stage3_index.sql` | 阶段3：索引功能测试 | multi-index, unique | ⭐⭐ 重要 |
| `test_plan_stage4_query.sql` | 阶段4：查询增强测试 | select-tables, join-tables, order-by, group-by, aggregation-func | ⭐⭐⭐ 关键 |
| `test_plan_stage5_advanced.sql` | 阶段5：高级功能测试 | simple-sub-query, update, insert(多行) | ⭐⭐ 重要 |
| `test_plan_regression.sql` | 回归测试 | 综合功能测试 | ⭐⭐⭐ 关键 |

### 文档文件

| 文件名 | 说明 |
|--------|------|
| `DEBUG_GUIDE.md` | 详细的调试指南，包含每个阶段的详细说明 |
| `QUICK_START_TEST.md` | 快速开始指南，适合快速上手 |
| `TEST_README.md` | 本文件，总体说明 |
| `run_debug_tests.py` | Python 自动化测试脚本（提供指导） |

---

## 🚀 快速开始

### 1. 最简单的方式（推荐新手）

```bash
# 1. 编译项目
bash build.sh debug

# 2. 阅读快速开始指南
cat QUICK_START_TEST.md

# 3. 按 F5 启动调试

# 4. 按顺序运行测试文件中的 SQL
```

### 2. 完整的测试流程

1. **阅读文档**：
   ```bash
   cat QUICK_START_TEST.md    # 快速入门
   cat DEBUG_GUIDE.md         # 详细指南
   ```

2. **编译项目**：
   ```bash
   bash build.sh debug
   ```

3. **运行测试**：
   - 方式A：F5 调试（推荐）
   - 方式B：CLI 命令行
   - 方式C：客户端-服务端

4. **按顺序测试**：
   ```
   阶段1 → 阶段2 → 阶段3 → 阶段4 → 阶段5 → 回归测试
   ```

---

## 📊 功能覆盖清单

### ✅ 已实现功能列表

- [x] **basic** - 基本功能（表、索引、查询）
- [x] **select-meta** - 元数据检查
- [x] **drop-table** - 删除表
- [x] **date** - 日期类型
- [x] **text** - 长文本类型（4096字节）
- [x] **null** - NULL值支持
- [x] **multi-index** - 多列索引
- [x] **unique** - 唯一索引
- [x] **select-tables** - 多表笛卡尔积
- [x] **join-tables** - INNER JOIN
- [x] **order-by** - 排序
- [x] **group-by** - 分组
- [x] **aggregation-func** - 聚合函数（MAX/MIN/COUNT/AVG）
- [x] **simple-sub-query** - 简单子查询
- [x] **update** - 更新操作
- [x] **insert** - 多行插入

### 功能依赖关系

```
基础层（必须先通过）
├── basic
├── select-meta
└── drop-table
    ↓
类型扩展层
├── date
├── text
└── null
    ↓
索引层
├── multi-index
└── unique
    ↓
查询增强层
├── select-tables
├── join-tables
├── order-by
└── group-by
    ↓
高级功能层
├── aggregation-func
├── simple-sub-query
├── update
└── insert (多行)
```

---

## 🎯 测试策略

### 分层测试

1. **基础功能测试** - 确保基础功能不被破坏
2. **类型扩展测试** - 验证新类型的正确性
3. **索引功能测试** - 验证索引的创建和使用
4. **查询增强测试** - 验证复杂查询的正确性
5. **高级功能测试** - 验证高级功能
6. **回归测试** - 验证功能组合

### 测试原则

1. **按顺序测试**：不要跳过阶段
2. **充分验证**：测试正常情况和边界情况
3. **记录问题**：发现问题及时记录
4. **性能关注**：功能正确 + 性能合理
5. **回归验证**：修复后重新测试

---

## 📖 使用示例

### 示例1：快速验证基础功能

```bash
# 1. 启动 observer
cd build_debug
./bin/observer -f ../etc/observer.ini -P cli

# 2. 运行基础测试
# 复制 test_plan_stage1_basic.sql 中的 SQL 逐条执行

# 3. 观察结果是否符合预期
```

### 示例2：调试 JOIN 功能

```bash
# 1. 在 VSCode 中打开项目
# 2. 在以下文件设置断点：
#    src/observer/sql/operator/join_physical_operator.cpp

# 3. 按 F5 启动调试

# 4. 在 DEBUG CONSOLE 中运行：
CREATE TABLE t1 (id INT, name CHAR(20));
CREATE TABLE t2 (id INT, value INT);
INSERT INTO t1 VALUES (1, 'Alice');
INSERT INTO t2 VALUES (1, 100);
SELECT * FROM t1 INNER JOIN t2 ON t1.id = t2.id;

# 5. 程序会在断点处停下，可以单步调试
```

### 示例3：批量回归测试

```bash
# 1. 逐个运行所有测试阶段
# 2. 记录每个阶段的测试结果
# 3. 如果某个阶段失败，修复后重新运行所有阶段
```

---

## 🔍 调试工具

### VSCode 调试（推荐）

- **优点**：可视化、断点、变量查看
- **适用**：开发调试、深入理解代码
- **方法**：按 F5 启动

### GDB 调试（Linux）

```bash
gdb ./build_debug/bin/observer
(gdb) run -f ../etc/observer.ini -P cli
(gdb) break parse_stage.cpp:45
(gdb) continue
```

### 日志调试

```bash
# 修改 etc/observer.ini
LOG_FILE_LEVEL=5
LOG_CONSOLE_LEVEL=3

# 查看日志
tail -f build_debug/observer.log
```

---

## 📝 测试报告模板

建议创建 `test_results.md` 记录测试结果：

```markdown
# 测试记录

## 日期：2024-XX-XX

### 测试环境
- 编译版本：Debug / Release
- 操作系统：Windows / Linux / macOS
- 编译器：GCC 11.x / Clang 14.x

### 测试结果

#### 阶段1: 基础功能
- [x] 表创建 - ✓ 通过
- [x] 数据插入 - ✓ 通过
- [x] 索引创建 - ✓ 通过
- [x] 表删除 - ✓ 通过

#### 阶段2: 类型扩展
- [x] DATE 类型 - ✓ 通过
- [ ] TEXT 类型 - ✗ 失败
  - 问题：超过4096字节未正确截断
  - 错误信息：...
  - 修复计划：...
- [x] NULL 支持 - ✓ 通过

...

### 发现的问题

1. **TEXT 类型截断问题**
   - 描述：...
   - 复现：...
   - 修复：...

2. ...

### 总结
- 通过测试：X 个
- 失败测试：Y 个
- 总耗时：Z 小时
```

---

## 🎓 学习建议

### 对于初学者

1. 先通读 `QUICK_START_TEST.md`
2. 按顺序运行每个阶段
3. 遇到问题查看 `DEBUG_GUIDE.md`
4. 使用 F5 调试理解代码

### 对于有经验者

1. 直接运行所有测试
2. 关注性能和边界情况
3. 进行压力测试
4. 优化代码实现

---

## 📈 性能基准

建议记录以下性能指标：

| 操作 | 数据量 | 预期时间 | 实际时间 |
|------|--------|----------|----------|
| 插入 | 1000行 | < 1s | |
| 查询 | 10000行 | < 0.1s | |
| JOIN | 1000x1000 | < 5s | |
| ORDER BY | 10000行 | < 1s | |
| GROUP BY | 10000行 | < 2s | |

---

## 🐛 问题排查指南

### 常见问题速查

| 问题现象 | 可能原因 | 排查方法 |
|----------|----------|----------|
| 段错误 | 空指针、数组越界 | GDB backtrace |
| 结果错误 | 逻辑错误、类型转换 | 断点调试 |
| 索引不生效 | 条件不匹配、统计信息 | 查看执行计划 |
| 性能差 | 未使用索引、算法问题 | profiling |
| 内存泄漏 | 未释放内存 | valgrind |

---

## 📞 获取帮助

1. **查看文档**：
   - `DEBUG_GUIDE.md` - 详细调试指南
   - `QUICK_START_TEST.md` - 快速开始
   
2. **查看代码**：
   - 各模块的注释和文档
   - `docs/docs/design/` 设计文档

3. **查看日志**：
   - `build_debug/observer.log`

4. **使用调试工具**：
   - VSCode 调试（F5）
   - GDB 调试
   - 日志输出

---

## ✅ 成功标准

一个功能被认为"测试通过"需要满足：

1. ✓ 功能正确：所有测试用例通过
2. ✓ 边界情况：边界值和异常输入处理正确
3. ✓ 性能合理：在可接受的时间内完成
4. ✓ 无内存泄漏：valgrind 检查通过
5. ✓ 代码质量：无编译警告，代码规范

---

## 🎉 恭喜

如果你完成了所有阶段的测试，恭喜你！你已经实现了一个功能相对完整的数据库系统。

接下来可以：
- 优化性能
- 实现更多功能
- 参加比赛
- 深入学习数据库内核

祝你在数据库学习之路上越走越远！🚀

