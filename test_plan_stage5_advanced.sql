-- ============================================
-- 阶段5: 高级功能测试
-- 测试目标: simple-sub-query, update, insert (多行)
-- ============================================

-- 准备测试数据
CREATE TABLE employees (id INT, name CHAR(20), age INT, salary FLOAT, dept_id INT);
CREATE TABLE departments (id INT, dept_name CHAR(30));

INSERT INTO employees VALUES (1, 'Alice', 30, 5000.0, 1);
INSERT INTO employees VALUES (2, 'Bob', 35, 6000.0, 1);
INSERT INTO employees VALUES (3, 'Charlie', 28, 4500.0, 2);
INSERT INTO employees VALUES (4, 'David', 40, 7000.0, 2);
INSERT INTO employees VALUES (5, 'Eve', 32, 5500.0, 3);

INSERT INTO departments VALUES (1, 'Engineering');
INSERT INTO departments VALUES (2, 'Sales');
INSERT INTO departments VALUES (3, 'HR');

-- ========== 测试1: INSERT 多行插入 ==========
CREATE TABLE test_insert (id INT, value INT);

-- 单次插入多行
INSERT INTO test_insert VALUES (1, 100), (2, 200), (3, 300);
SELECT * FROM test_insert;
SELECT COUNT(*) FROM test_insert;  -- 应该是3

-- 多行插入应该是原子操作（全部成功或全部失败）
-- 如果有唯一约束
CREATE TABLE test_insert_unique (id INT);
CREATE UNIQUE INDEX idx_id ON test_insert_unique(id);
INSERT INTO test_insert_unique VALUES (1), (2), (3);
-- 下面应该全部失败（因为id=1已存在）
INSERT INTO test_insert_unique VALUES (4), (1), (5);  
SELECT COUNT(*) FROM test_insert_unique;  -- 应该还是3

-- ========== 测试2: UPDATE 基础功能 ==========
-- 不带条件的更新
CREATE TABLE test_update (id INT, value INT);
INSERT INTO test_update VALUES (1, 10);
INSERT INTO test_update VALUES (2, 20);
INSERT INTO test_update VALUES (3, 30);

UPDATE test_update SET value = 100;
SELECT * FROM test_update;  -- 所有value应该都是100

-- 带条件的更新
UPDATE test_update SET value = 50 WHERE id = 2;
SELECT * FROM test_update WHERE id = 2;  -- value应该是50

UPDATE test_update SET value = 60 WHERE id > 1;
SELECT * FROM test_update ORDER BY id;  -- id>1的value应该是60

-- 更新为原值的表达式（如果支持）
-- UPDATE test_update SET value = value + 10 WHERE id = 1;

-- ========== 测试3: SIMPLE-SUB-QUERY ==========
-- IN 子查询
SELECT * FROM employees WHERE dept_id IN (SELECT id FROM departments WHERE dept_name = 'Engineering');
SELECT * FROM employees WHERE dept_id IN (SELECT id FROM departments);

-- NOT IN 子查询
SELECT * FROM employees WHERE dept_id NOT IN (SELECT id FROM departments WHERE id = 1);

-- 比较运算子查询
SELECT * FROM employees WHERE age > (SELECT AVG(age) FROM employees);
SELECT * FROM employees WHERE salary > (SELECT MAX(salary) FROM employees WHERE dept_id = 1);

-- 子查询带聚合函数
SELECT * FROM employees WHERE age > (SELECT AVG(age) FROM employees);
SELECT * FROM employees WHERE salary < (SELECT AVG(salary) FROM employees WHERE dept_id = 2);

-- 复杂子查询
SELECT * FROM employees WHERE age > (SELECT AVG(age) FROM employees) AND salary > 5000.0;

-- 子查询在多条件中
SELECT * FROM employees 
WHERE age > (SELECT AVG(age) FROM employees) 
  AND dept_id IN (SELECT id FROM departments WHERE id > 1);

-- 嵌套子查询
SELECT * FROM employees 
WHERE dept_id IN (
  SELECT id FROM departments 
  WHERE id IN (SELECT dept_id FROM employees WHERE age > 30)
);

-- ========== 测试4: UPDATE + 子查询 ==========
-- 使用子查询结果更新
UPDATE employees SET salary = 8000.0 
WHERE age > (SELECT AVG(age) FROM employees);
SELECT * FROM employees WHERE salary = 8000.0;

-- ========== 测试5: 综合测试 ==========
-- 多行插入 + 子查询 + 聚合
CREATE TABLE temp_emp (id INT, avg_salary FLOAT);
INSERT INTO temp_emp VALUES (1, 5000.0), (2, 6000.0);

SELECT * FROM employees 
WHERE salary > (SELECT AVG(avg_salary) FROM temp_emp);

-- UPDATE + JOIN (如果支持)
-- UPDATE employees SET salary = 9000.0 
-- WHERE dept_id IN (SELECT id FROM departments WHERE dept_name = 'Engineering');

-- 清理
DROP TABLE test_insert;
DROP TABLE test_insert_unique;
DROP TABLE test_update;
DROP TABLE temp_emp;
DROP TABLE employees;
DROP TABLE departments;

