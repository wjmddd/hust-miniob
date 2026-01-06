/**
 * @file large_object_manager.h
 * @brief 大对象管理器（Large Object Manager, LOM）
 * 
 * 【text功能核心 - TEXT类型存储管理】
 * 
 * 设计背景：
 * - miniob的普通记录是定长存储，不适合存储大文本
 * - TEXT类型最大4096字节，需要独立存储
 * - 通过LOM管理TEXT数据，记录中只保存索引
 * 
 * 使用方式：
 * 1. INSERT时：调用add_obj添加文本，获取索引存入记录
 * 2. SELECT时：从记录获取索引，调用find_obj获取文本
 * 3. UPDATE时：调用update_obj更新文本内容
 * 4. DROP TABLE时：调用drop删除所有TEXT数据
 * 
 * 存储策略：
 * - 内存：std::vector<std::string>，按索引访问
 * - 磁盘：二进制文件，[长度][内容][长度][内容]...
 */

#pragma once

/**
 * @brief TEXT类型的最大长度（4096字节）
 * 
 * 题目要求：text长度固定4096字节
 * 超过此长度的文本会被截断
 */
#define OBJ_MAX_LENGTH 4096

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "common/sys/rc.h"

/**
 * @class LOM
 * @brief 大对象管理器类（Large Object Manager）
 * 
 * 【text功能核心类】
 * 
 * 管理TEXT类型的大文本数据：
 * - 每个表有一个LOM实例
 * - TEXT数据通过索引（uint32_t）引用
 * - 索引存储在记录中，实际文本存储在LOM中
 */
class LOM
{
public:
  LOM()  = default;
  ~LOM() = default;

  /**
   * @brief 设置LOM文件路径
   * @param path 文件路径（通常为 表名.text）
   */
  void set_path(std::string path);
  
  /**
   * @brief 打开LOM文件，加载数据到内存
   * @return RC 成功返回SUCCESS
   */
  RC   open();
  
  /**
   * @brief 删除LOM文件和内存数据
   * @return RC 成功返回SUCCESS
   */
  RC   drop();
  
  /**
   * @brief 将内存数据刷新到磁盘
   * @return RC 成功返回SUCCESS
   */
  RC   flush();
  
  /**
   * @brief 添加新的文本对象
   * @param obj   文本内容（超过4096字节会被截断）
   * @param index 输出：新文本的索引
   * @return RC   成功返回SUCCESS
   */
  RC   add_obj(std::string obj, uint32_t &index);
  
  /**
   * @brief 根据索引查找文本内容
   * @param obj   输出：文本内容
   * @param index 文本索引
   * @return RC   成功返回SUCCESS，索引无效返回NOT_EXIST
   */
  RC   find_obj(std::string &obj, uint32_t index) const;
  
  /**
   * @brief 更新指定索引的文本内容
   * @param new_obj 新的文本内容
   * @param index   要更新的索引
   * @return RC     成功返回SUCCESS
   */
  RC   update_obj(const std::string &new_obj, uint32_t index);

private:
  std::string              path_;     ///< LOM文件路径
  std::vector<std::string> values_;   ///< 内存中的文本数据，索引即为数组下标
};