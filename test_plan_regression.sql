-- ============================================
-- 回归测试：验证功能组合不会相互破坏
-- ============================================

-- 测试1: DATE + NULL + UNIQUE
CREATE TABLE users (
  id INT NOT NULL, 
  username CHAR(30) NOT NULL, 
  email CHAR(50) NULLABLE,
  birthday DATE,
  created_at DATE
);

CREATE UNIQUE INDEX idx_username ON users(username);
CREATE INDEX idx_birthday ON users(birthday);

INSERT INTO users VALUES (1, 'alice', 'alice@test.com', '1990-01-01', '2024-01-01');
INSERT INTO users VALUES (2, 'bob', NULL, '1992-05-15', '2024-01-02');
INSERT INTO users VALUES (3, 'charlie', 'charlie@test.com', NULL, '2024-01-03');

SELECT * FROM users WHERE birthday > '1991-01-01';
SELECT * FROM users WHERE email = 'alice@test.com';

-- 测试2: TEXT + GROUP BY + ORDER BY
CREATE TABLE articles (
  id INT,
  title CHAR(100),
  content TEXT,
  author CHAR(30),
  publish_date DATE
);

INSERT INTO articles VALUES (1, 'Article 1', 'This is content 1', 'Alice', '2024-01-01');
INSERT INTO articles VALUES (2, 'Article 2', 'This is content 2', 'Bob', '2024-01-02');
INSERT INTO articles VALUES (3, 'Article 3', 'This is content 3', 'Alice', '2024-01-03');
INSERT INTO articles VALUES (4, 'Article 4', 'This is content 4', 'Charlie', '2024-01-04');

SELECT author, COUNT(*) FROM articles GROUP BY author ORDER BY COUNT(*) DESC;
SELECT * FROM articles WHERE publish_date > '2024-01-01' ORDER BY publish_date DESC;

-- 测试3: MULTI-INDEX + JOIN + 子查询
CREATE TABLE orders (
  order_id INT,
  customer_id INT,
  product_id INT,
  order_date DATE,
  amount FLOAT
);

CREATE TABLE customers (
  customer_id INT,
  customer_name CHAR(30),
  register_date DATE
);

CREATE INDEX idx_order_customer_date ON orders(customer_id, order_date);

INSERT INTO orders VALUES (1, 1, 100, '2024-01-01', 299.99);
INSERT INTO orders VALUES (2, 1, 101, '2024-01-05', 149.99);
INSERT INTO orders VALUES (3, 2, 100, '2024-01-03', 299.99);
INSERT INTO orders VALUES (4, 3, 102, '2024-01-10', 499.99);

INSERT INTO customers VALUES (1, 'Customer A', '2023-01-01');
INSERT INTO customers VALUES (2, 'Customer B', '2023-06-15');
INSERT INTO customers VALUES (3, 'Customer C', '2023-12-01');

-- JOIN + GROUP BY + 聚合
SELECT customers.customer_name, COUNT(*), SUM(orders.amount)
FROM orders 
INNER JOIN customers ON orders.customer_id = customers.customer_id
GROUP BY customers.customer_name;

-- 子查询 + 多表
SELECT * FROM orders 
WHERE customer_id IN (
  SELECT customer_id FROM customers WHERE register_date > '2023-06-01'
);

-- 测试4: UPDATE + 多列索引 + NULL
UPDATE users SET email = 'newemail@test.com' WHERE username = 'alice';
SELECT * FROM users WHERE username = 'alice';

UPDATE orders SET amount = 0.0 WHERE order_date < '2024-01-05';
SELECT * FROM orders WHERE amount = 0.0;

-- 测试5: 多行INSERT + UNIQUE + 原子性
CREATE TABLE test_atomic (id INT, value INT);
CREATE UNIQUE INDEX idx_atomic_id ON test_atomic(id);

INSERT INTO test_atomic VALUES (1, 10), (2, 20), (3, 30);
SELECT COUNT(*) FROM test_atomic;  -- 应该是3

-- 这个应该失败（id=2已存在），所有插入都应该回滚
INSERT INTO test_atomic VALUES (4, 40), (2, 25), (5, 50);
SELECT COUNT(*) FROM test_atomic;  -- 应该还是3

-- 测试6: 复杂子查询 + 聚合 + JOIN
SELECT customers.customer_name, 
       (SELECT AVG(amount) FROM orders WHERE orders.customer_id = customers.customer_id) as avg_amount
FROM customers
WHERE customers.customer_id IN (
  SELECT customer_id FROM orders 
  WHERE amount > (SELECT AVG(amount) FROM orders)
);

-- 测试7: 所有类型组合
CREATE TABLE comprehensive_test (
  id INT NOT NULL,
  name CHAR(50) NOT NULL,
  description TEXT NULLABLE,
  birth_date DATE,
  created_date DATE,
  score FLOAT
);

CREATE UNIQUE INDEX idx_comp_id ON comprehensive_test(id);
CREATE INDEX idx_comp_dates ON comprehensive_test(birth_date, created_date);

INSERT INTO comprehensive_test VALUES 
  (1, 'Test1', 'Description 1', '1990-01-01', '2024-01-01', 85.5),
  (2, 'Test2', NULL, '1995-05-15', '2024-01-02', 90.0),
  (3, 'Test3', 'Description 3', '2000-12-31', '2024-01-03', 78.5);

SELECT * FROM comprehensive_test 
WHERE birth_date > '1990-01-01' AND score > 80.0
ORDER BY score DESC;

SELECT AVG(score) FROM comprehensive_test 
WHERE created_date > '2024-01-01';

-- 清理
DROP TABLE users;
DROP TABLE articles;
DROP TABLE orders;
DROP TABLE customers;
DROP TABLE test_atomic;
DROP TABLE comprehensive_test;

