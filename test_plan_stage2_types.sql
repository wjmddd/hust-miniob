-- ============================================
-- 阶段2: 类型扩展测试
-- 测试目标: date, text, null
-- ============================================

-- 测试1: DATE 类型基础功能
CREATE TABLE t_date (id INT, birthday DATE, join_date DATE);
INSERT INTO t_date VALUES (1, '2000-01-01', '2020-03-15');
INSERT INTO t_date VALUES (2, '1995-06-30', '2021-12-31');
INSERT INTO t_date VALUES (3, '2024-02-29', '2024-02-29');  -- 闰年
SELECT * FROM t_date;

-- 测试2: DATE 比较和索引
SELECT * FROM t_date WHERE birthday > '1996-01-01';
SELECT * FROM t_date WHERE birthday = '2000-01-01';
CREATE INDEX idx_birthday ON t_date(birthday);
SELECT * FROM t_date WHERE birthday > '1996-01-01';  -- 使用索引

-- 测试3: DATE 非法输入 (应该返回FAILURE)
INSERT INTO t_date VALUES (4, '2024-02-30', '2020-01-01');  -- 无效日期
INSERT INTO t_date VALUES (5, '2023-02-29', '2020-01-01');  -- 非闰年

-- 测试4: TEXT 类型基础功能
CREATE TABLE t_text (id INT, content TEXT);
INSERT INTO t_text VALUES (1, 'short text');
INSERT INTO t_text VALUES (2, 'This is a very long text that might be longer than normal char fields can support. It should be able to store up to 4096 bytes of data without any issues.');
SELECT * FROM t_text;

-- 测试5: TEXT 字段查询
SELECT * FROM t_text WHERE id = 1;
SELECT id FROM t_text WHERE content = 'short text';

-- 测试6: TEXT 超长截断 (应该截断到4096字节)
INSERT INTO t_text VALUES (3, 'x repeated string...');  -- 假设超过4096字节

-- 测试7: NULL 基础功能
CREATE TABLE t_null (id INT NOT NULL, age INT NOT NULL, address CHAR(50) NULLABLE);
INSERT INTO t_null VALUES (1, 20, 'Beijing');
INSERT INTO t_null VALUES (2, 25, NULL);
INSERT INTO t_null VALUES (3, 30, null);  -- 小写
SELECT * FROM t_null;

-- 测试8: NULL 比较规则 (NULL与任何数据比较都是FALSE)
SELECT * FROM t_null WHERE address = NULL;      -- 应该返回空
SELECT * FROM t_null WHERE address != NULL;     -- 应该返回空
SELECT * FROM t_null WHERE address = 'Beijing'; -- 应该只返回id=1

-- 测试9: NOT NULL 约束
INSERT INTO t_null VALUES (NULL, 30, 'Shanghai');  -- 应该失败
INSERT INTO t_null VALUES (4, NULL, 'Shanghai');   -- 应该失败

-- 测试10: NULL与聚合函数
CREATE TABLE t_null_agg (id INT, value INT NULLABLE);
INSERT INTO t_null_agg VALUES (1, 10);
INSERT INTO t_null_agg VALUES (2, NULL);
INSERT INTO t_null_agg VALUES (3, 20);
INSERT INTO t_null_agg VALUES (4, NULL);
SELECT COUNT(*) FROM t_null_agg;     -- 应该是4
SELECT COUNT(value) FROM t_null_agg; -- 应该是2 (NULL不计入)
SELECT AVG(value) FROM t_null_agg;   -- 应该是15 (只计算非NULL)

-- 清理
DROP TABLE t_date;
DROP TABLE t_text;
DROP TABLE t_null;
DROP TABLE t_null_agg;

