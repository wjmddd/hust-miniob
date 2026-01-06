/**
 * @file large_object_manager.cpp
 * @brief 大对象管理器（LOM）实现
 * 
 * 【text功能核心实现文件 - 大文本存储】
 * 
 * 主要功能：
 * 1. 管理TEXT类型字段的大文本数据
 * 2. 提供独立于记录的文本存储机制
 * 3. 支持文本的增删改查操作
 * 
 * 设计背景：
 * - TEXT类型最大长度4096字节，超出普通字段存储限制
 * - 普通记录按固定长度存储，不适合变长大文本
 * - 使用独立文件存储TEXT数据，记录中只保存索引
 * 
 * 存储格式：
 * - 文件格式：[长度1][数据1][长度2][数据2]...
 * - 长度：4字节无符号整数（uint32_t）
 * - 数据：实际文本内容
 * 
 * 记录与TEXT的关联：
 * - 记录中存储TEXT的索引（index）
 * - 通过索引在LOM中查找实际文本内容
 */

#include "large_object_manager.h"

// LOM::~LOM()
// {
//   flush();
//   values_.clear();
//   std::remove(path_.c_str());
// }

/**
 * @brief 设置LOM文件路径
 * @param path 文件路径
 */
void LOM::set_path(std::string path) { path_.assign(path); }

/**
 * @brief 打开LOM文件，加载所有文本数据到内存
 * 
 * 【text功能 - 数据加载】
 * 
 * @return RC 成功返回SUCCESS，文件无法打开返回FILE_CLOSE
 * 
 * 加载过程：
 * 1. 以二进制模式打开文件
 * 2. 循环读取：先读长度，再读数据
 * 3. 所有文本加载到values_向量中
 */
RC LOM::open()
{
  std::ifstream file(path_, std::ios::binary);
  if (not file.is_open()) {
    return RC::FILE_CLOSE;
  }
  // printf("lom open.\n");
  values_.clear();  // 清空现有数据
  
  // 循环读取所有文本
  while (file.peek() != EOF) {
    // 第1步：读取字符串长度（4字节）
    uint32_t length = 0;
    file.read(reinterpret_cast<char *>(&length), sizeof(length));
    
    // 第2步：读取字符串数据
    std::string str(length, '\0');  // 预分配字符串长度
    file.read(&str[0], length);
    
    // 添加到向量中，索引即为数组下标
    values_.push_back(std::move(str));
  }
  file.close();
  return RC::SUCCESS;
}

/**
 * @brief 删除LOM文件和内存数据
 * 
 * 【text功能 - 清理资源】
 * 
 * @return RC 始终返回SUCCESS
 * 
 * 用于DROP TABLE时清理TEXT相关数据
 */
RC LOM::drop()
{
  values_.clear();              // 清空内存数据
  std::remove(path_.c_str());   // 删除磁盘文件
  return RC::SUCCESS;
}

/**
 * @brief 将内存数据刷新到磁盘
 * 
 * 【text功能 - 数据持久化】
 * 
 * @return RC 成功返回SUCCESS，文件无法打开返回FILE_CLOSE
 * 
 * 存储格式：
 * - 每个文本：[4字节长度][文本内容]
 * - 使用trunc模式，每次写入覆盖整个文件
 */
RC LOM::flush()
{
  // 以二进制+截断模式打开，完全覆盖原文件
  std::ofstream file(path_, std::ios::binary | std::ios::trunc);
  if (not file.is_open()) {
    return RC::FILE_CLOSE;
  }
  // printf("lom flush.\n");
  
  // 写入所有字符串
  for (const auto &str : values_) {
    // 第1步：写入字符串长度（4字节）
    uint32_t length = static_cast<uint32_t>(str.size());
    file.write(reinterpret_cast<const char *>(&length), sizeof(length));
    
    // 第2步：写入字符串数据
    file.write(str.data(), length);
  }
  file.close();
  return RC::SUCCESS;
}

/**
 * @brief 添加新的文本对象
 * 
 * 【text功能核心 - 文本插入】
 * 
 * @param obj   要添加的文本内容
 * @param index 输出：新文本的索引（存入记录中）
 * @return RC   始终返回SUCCESS
 * 
 * 处理逻辑：
 * 1. 如果文本超过OBJ_MAX_LENGTH(4096)字节，截断
 * 2. 添加到values_向量末尾
 * 3. 返回索引（数组下标）供记录引用
 * 
 * 注意：这里的截断符合题目要求：
 * "如果输入的字符串长度超过4096，那么应该保存4096字节，剩余的数据截断"
 */
RC LOM::add_obj(std::string obj, uint32_t &index)
{
  std::string s;
  s.assign(obj);
  // 【text功能关键】超长截断处理
  if (s.size() > OBJ_MAX_LENGTH) {
    s = s.substr(0, OBJ_MAX_LENGTH);  // 只保留前 OBJ_MAX_LENGTH(4096) 字节
  }
  values_.push_back(s);
  index = values_.size() - 1;  // 索引为数组下标
  return RC::SUCCESS;
}

/**
 * @brief 根据索引查找文本内容
 * 
 * 【text功能核心 - 文本读取】
 * 
 * @param obj   输出：文本内容
 * @param index 文本索引（从记录中获取）
 * @return RC   成功返回SUCCESS，索引无效返回NOT_EXIST
 * 
 * 用于SELECT查询时获取TEXT字段的实际内容
 */
RC LOM::find_obj(std::string &obj, uint32_t index) const
{
  // 检查索引是否有效
  if (index >= values_.size()) {
    return RC::NOT_EXIST;
  }
  obj.assign(values_[index]);
  return RC::SUCCESS;
}

/**
 * @brief 更新指定索引的文本内容
 * 
 * 【text功能 - 文本更新】
 * 
 * @param new_obj 新的文本内容
 * @param index   要更新的文本索引
 * @return RC     成功返回SUCCESS，索引无效返回NOT_EXIST
 * 
 * 用于UPDATE语句更新TEXT字段
 * 同样会进行超长截断处理
 */
RC LOM::update_obj(const std::string &new_obj, uint32_t index)
{
  // 检查索引是否有效
  if (index >= values_.size()) {
    return RC::NOT_EXIST;
  }
  std::string s;
  s.assign(new_obj);
  // 【text功能关键】超长截断处理
  if (s.size() > OBJ_MAX_LENGTH) {
    s = s.substr(0, OBJ_MAX_LENGTH);  // 只保留前 OBJ_MAX_LENGTH(4096) 字节
  }
  values_[index].assign(s);
  return RC::SUCCESS;
}