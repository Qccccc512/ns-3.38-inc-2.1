#ifndef INC_H
#define INC_H

#include "inc-header.h"


/**
 * \defgroup inc 在网计算模块
 * \brief 实现在网计算功能的NS3模块
 * 
 * 该模块提供了在网计算的功能实现,主要包括:
 * - 在网计算协议头(IncHeader)
 * - 在网计算协议的传输、接收、处理等功能的实现
 * - 相关的帮助类
 */

namespace ns3
{

/**
 * \ingroup inc
 * \brief 在网计算协议的命名空间常量和全局函数
 */

// 常数定义
constexpr uint16_t INC_DEFAULT_PORT = 9; // 默认在网计算端口(传输层)



}

#endif /* INC_H */
