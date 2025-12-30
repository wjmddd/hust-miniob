-- ============================================
-- 阶段3: 索引功能测试
-- 测试目标: multi-index, unique
-- ============================================

-- 测试1: 多列索引基础功能
CREATE TABLE t_multi (id INT, age INT, name CHAR(20));
INSERT INTO t_multi VALUES (1, 20, 'Alice');
INSERT INTO t_multi VALUES (2, 20, 'Bob');
INSERT INTO t_multi VALUES (3, 25, 'Charlie');
INSERT INTO t_multi VALUES (4, 25, 'David');

-- 创建多列索引
CREATE INDEX idx_age_name ON t_multi(age, name);

-- 测试2: 多列索引查询 (前缀匹配)
SELECT * FROM t_multi WHERE age = 20;  -- 应该使用索引
SELECT * FROM t_multi WHERE age = 20 AND name = 'Alice';  -- 应该使用索引

-- 测试3: 多列索引查询 (不使用前缀，可能不使用索引)
SELECT * FROM t_multi WHERE name = 'Alice';  -- 可能不使用索引

-- 测试4: 多列索引排序效果
CREATE INDEX idx_id_age ON t_multi(id, age);
SELECT * FROM t_multi WHERE id > 1;

-- 测试5: UNIQUE 索引基础功能
CREATE TABLE t_unique (id INT, email CHAR(50));
CREATE UNIQUE INDEX idx_email ON t_unique(email);

INSERT INTO t_unique VALUES (1, 'alice@test.com');
INSERT INTO t_unique VALUES (2, 'bob@test.com');
SELECT * FROM t_unique;

-- 测试6: UNIQUE 约束检查 (应该失败)
INSERT INTO t_unique VALUES (3, 'alice@test.com');  -- 重复email，应该失败

-- 测试7: UNIQUE 索引更新
INSERT INTO t_unique VALUES (3, 'charlie@test.com');
SELECT * FROM t_unique WHERE email = 'charlie@test.com';

-- 测试8: UNIQUE多列索引
CREATE TABLE t_unique_multi (first_name CHAR(20), last_name CHAR(20), age INT);
CREATE UNIQUE INDEX idx_name ON t_unique_multi(first_name, last_name);

INSERT INTO t_unique_multi VALUES ('John', 'Doe', 30);
INSERT INTO t_unique_multi VALUES ('John', 'Smith', 25);  -- 应该成功
INSERT INTO t_unique_multi VALUES ('Jane', 'Doe', 28);    -- 应该成功
INSERT INTO t_unique_multi VALUES ('John', 'Doe', 35);    -- 应该失败，重复

-- 测试9: UNIQUE 和 NULL (如果实现了NULL)
CREATE TABLE t_unique_null (id INT, email CHAR(50) NULLABLE);
CREATE UNIQUE INDEX idx_email_null ON t_unique_null(email);
INSERT INTO t_unique_null VALUES (1, 'test@test.com');
INSERT INTO t_unique_null VALUES (2, NULL);
INSERT INTO t_unique_null VALUES (3, NULL);  -- NULL可以重复吗？取决于实现

-- 清理
DROP TABLE t_multi;
DROP TABLE t_unique;
DROP TABLE t_unique_multi;
DROP TABLE t_unique_null;

