/**
 * @file update_physical_operator.cpp
 * @brief UPDATE物理操作符实现（Executor阶段）
 * 
 * 【update功能核心实现文件 - Executor阶段】
 * 
 * 主要功能：
 * 1. 执行UPDATE语句的实际数据修改
 * 2. 使用Volcano模型遍历需要更新的记录
 * 3. 调用事务层的update_record完成记录更新
 * 
 * 执行流程：
 *   UpdatePhysicalOperator::open()
 *     ├── 打开子操作符（通常是TableScanOperator + PredicateOperator）
 *     ├── 遍历并收集所有匹配WHERE条件的记录
 *     └── 对每条记录调用trx_->update_record()执行更新
 * 
 * 为什么先收集再更新？
 *   - 避免在遍历过程中修改数据导致游标失效
 *   - 支持事务回滚（如果部分更新失败）
 */

#include "sql/operator/update_physical_operator.h"
#include "common/log/log.h"
#include "storage/table/table.h"
#include "storage/trx/trx.h"

/**
 * @brief 打开UPDATE物理操作符，执行更新操作
 * 
 * 【update功能执行入口】
 * 
 * @param trx 当前事务指针
 * @return RC 成功返回SUCCESS，失败返回对应错误码
 * 
 * 执行步骤：
 * 1. 打开子操作符（扫描+过滤）
 * 2. 收集所有需要更新的记录
 * 3. 关闭子操作符
 * 4. 对每条记录执行更新
 */
RC UpdatePhysicalOperator::open(Trx *trx)
{
  // 如果没有子操作符，直接返回成功（无数据可更新）
  if (children_.empty()) {
    return RC::SUCCESS;
  }

  // 获取子操作符（通常是 PredicateOperator，用于过滤WHERE条件）
  std::unique_ptr<PhysicalOperator> &child = children_[0];

  // 第1步：打开子操作符，初始化扫描
  RC rc = child->open(trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open child operator: %s", strrc(rc));
    return rc;
  }

  // 保存事务指针，后续更新时需要
  trx_ = trx;

  // 第2步：遍历子操作符，收集所有匹配的记录
  // 使用Volcano模型：循环调用next()获取下一条记录
  while (OB_SUCC(rc = child->next())) {
    // 获取当前元组
    Tuple *tuple = child->current_tuple();
    if (nullptr == tuple) {
      return rc;
    }

    // 转换为RowTuple，获取底层Record
    RowTuple *row_tuple = static_cast<RowTuple *>(tuple);
    Record   &record    = row_tuple->record();
    // 收集记录到records_列表（使用move语义提高效率）
    records_.emplace_back(std::move(record));
  }

  // 第3步：关闭子操作符
  child->close();

  // 第4步：执行实际的更新操作
  // 先收集记录再更新的原因：
  // 1. 避免在遍历过程中修改数据导致游标失效
  // 2. 记录的有效性由事务来保证
  // 3. 如果事务不保证更新的有效性，那说明此事务类型不支持并发控制，比如VacuousTrx
  for (Record &record : records_) {
    // 调用事务层的update_record方法执行更新
    // 参数：表对象、记录、字段名、新值
    // 内部实现通常是：删除旧记录 + 插入新记录（Delete + Insert策略）
    rc = trx_->update_record(table_, record, field_meta_->name(), value_);
    if (rc != RC::SUCCESS) {
      // 更新失败，返回错误（事务层会处理回滚）
      return rc;
    }
  }

  return RC::SUCCESS;
}

/**
 * @brief 获取下一条结果
 * @return RC UPDATE操作不返回结果集，直接返回RECORD_EOF
 * @details UPDATE是DML操作，不需要返回数据给客户端
 */
RC UpdatePhysicalOperator::next() { return RC::RECORD_EOF; }

/**
 * @brief 关闭操作符，释放资源
 * @return RC 始终返回SUCCESS
 */
RC UpdatePhysicalOperator::close() { return RC::SUCCESS; }
