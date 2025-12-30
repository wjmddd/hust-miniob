-- ============================================
-- 阶段1: 基础功能测试
-- 测试目标: basic, select-meta, drop-table
-- ============================================

-- 测试1: 基本的表创建和插入
CREATE TABLE t_basic (id INT, name CHAR(20), age INT);
INSERT INTO t_basic VALUES (1, 'Alice', 20);
INSERT INTO t_basic VALUES (2, 'Bob', 25);
SELECT * FROM t_basic;

-- 测试2: 条件查询
SELECT * FROM t_basic WHERE id = 1;
SELECT * FROM t_basic WHERE age > 20;
SELECT * FROM t_basic WHERE id = 1 AND age = 20;

-- 测试3: 索引功能
CREATE INDEX idx_id ON t_basic(id);
SELECT * FROM t_basic WHERE id = 1;  -- 应该使用索引
CREATE INDEX idx_age ON t_basic(age);
SELECT * FROM t_basic WHERE age > 20;

-- 测试4: select-meta (元数据检查 - 应该失败)
SELECT nonexist_col FROM t_basic;  -- 应该返回FAILURE
SELECT * FROM nonexist_table;       -- 应该返回FAILURE
SELECT t_basic.id, t_basic.nonexist FROM t_basic;  -- 应该返回FAILURE

-- 测试5: drop-table
DROP TABLE t_basic;
SELECT * FROM t_basic;  -- 应该失败，表已删除

-- 测试6: 确保索引也被删除
CREATE TABLE t_drop (id INT, name CHAR(10));
CREATE INDEX idx_drop ON t_drop(id);
INSERT INTO t_drop VALUES (1, 'test');
DROP TABLE t_drop;
-- 重新创建同名表，应该可以成功
CREATE TABLE t_drop (id INT, age INT);
INSERT INTO t_drop VALUES (1, 20);
SELECT * FROM t_drop;

CLEANUP:
DROP TABLE t_drop;

