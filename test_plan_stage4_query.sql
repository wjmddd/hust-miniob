-- ============================================
-- 阶段4: 查询增强测试
-- 测试目标: select-tables, join-tables, order-by, group-by, aggregation-func
-- ============================================

-- 准备测试数据
CREATE TABLE students (id INT, name CHAR(20), age INT, class_id INT);
CREATE TABLE classes (id INT, class_name CHAR(30), teacher CHAR(20));
CREATE TABLE scores (student_id INT, subject CHAR(20), score FLOAT);

INSERT INTO students VALUES (1, 'Alice', 20, 1);
INSERT INTO students VALUES (2, 'Bob', 22, 1);
INSERT INTO students VALUES (3, 'Charlie', 21, 2);
INSERT INTO students VALUES (4, 'David', 23, 2);

INSERT INTO classes VALUES (1, 'Class A', 'Mr. Smith');
INSERT INTO classes VALUES (2, 'Class B', 'Ms. Johnson');

INSERT INTO scores VALUES (1, 'Math', 90.5);
INSERT INTO scores VALUES (1, 'English', 85.0);
INSERT INTO scores VALUES (2, 'Math', 88.0);
INSERT INTO scores VALUES (2, 'English', 92.0);
INSERT INTO scores VALUES (3, 'Math', 95.0);
INSERT INTO scores VALUES (4, 'Math', 78.5);

-- ========== 测试1: SELECT-TABLES (笛卡尔积) ==========
SELECT * FROM students, classes;  -- 应该返回 4*2=8 行
SELECT students.id, students.name, classes.class_name FROM students, classes;
SELECT students.id, students.name, classes.class_name FROM students, classes WHERE students.class_id = classes.id;

-- 测试不等比较符号
SELECT * FROM students, classes WHERE students.id <> classes.id;
SELECT * FROM students, classes WHERE students.id != classes.id;

-- ========== 测试2: JOIN-TABLES ==========
-- INNER JOIN 基础
SELECT * FROM students INNER JOIN classes ON students.class_id = classes.id;
SELECT students.name, classes.class_name FROM students INNER JOIN classes ON students.class_id = classes.id;

-- 多表 JOIN
SELECT students.name, classes.class_name, scores.subject, scores.score 
FROM students 
INNER JOIN classes ON students.class_id = classes.id 
INNER JOIN scores ON students.id = scores.student_id;

-- 带多个ON条件的JOIN
SELECT * FROM students 
INNER JOIN scores ON students.id = scores.student_id AND scores.subject = 'Math';

-- ========== 测试3: ORDER BY ==========
SELECT * FROM students ORDER BY age;       -- 默认升序
SELECT * FROM students ORDER BY age ASC;   -- 显式升序
SELECT * FROM students ORDER BY age DESC;  -- 降序
SELECT * FROM students ORDER BY name;      -- 字符串排序

-- 多列排序
SELECT * FROM students ORDER BY class_id, age;
SELECT * FROM students ORDER BY class_id DESC, age ASC;

-- 带条件的ORDER BY
SELECT * FROM students WHERE age > 20 ORDER BY age DESC;

-- ========== 测试4: AGGREGATION-FUNC ==========
-- COUNT
SELECT COUNT(*) FROM students;
SELECT COUNT(id) FROM students;

-- MAX/MIN
SELECT MAX(age) FROM students;
SELECT MIN(age) FROM students;
SELECT MAX(score) FROM scores;
SELECT MIN(score) FROM scores;

-- AVG
SELECT AVG(age) FROM students;
SELECT AVG(score) FROM scores;
SELECT AVG(score) FROM scores WHERE subject = 'Math';

-- 聚合函数与条件
SELECT COUNT(*) FROM students WHERE age > 20;
SELECT AVG(score) FROM scores WHERE student_id = 1;

-- ========== 测试5: GROUP BY ==========
-- 基础GROUP BY
SELECT class_id, COUNT(*) FROM students GROUP BY class_id;
SELECT subject, AVG(score) FROM scores GROUP BY subject;
SELECT subject, MAX(score) FROM scores GROUP BY subject;
SELECT subject, MIN(score) FROM scores GROUP BY subject;

-- GROUP BY 多列
SELECT class_id, COUNT(*) FROM students GROUP BY class_id;

-- GROUP BY 与 ORDER BY 结合
SELECT subject, AVG(score) FROM scores GROUP BY subject ORDER BY AVG(score) DESC;

-- 复杂GROUP BY
SELECT student_id, COUNT(*), AVG(score) FROM scores GROUP BY student_id;

-- ========== 测试6: 综合查询 ==========
-- JOIN + WHERE + ORDER BY
SELECT students.name, classes.class_name, students.age 
FROM students 
INNER JOIN classes ON students.class_id = classes.id 
WHERE students.age > 20 
ORDER BY students.age DESC;

-- JOIN + GROUP BY + 聚合
SELECT classes.class_name, COUNT(*), AVG(students.age)
FROM students 
INNER JOIN classes ON students.class_id = classes.id 
GROUP BY classes.class_name;

-- 多表JOIN + 聚合
SELECT students.name, AVG(scores.score) 
FROM students 
INNER JOIN scores ON students.id = scores.student_id 
GROUP BY students.name;

-- 清理
DROP TABLE students;
DROP TABLE classes;
DROP TABLE scores;

