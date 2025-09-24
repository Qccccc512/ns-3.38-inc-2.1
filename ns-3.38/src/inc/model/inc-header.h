#ifndef INC_HEADER_H
#define INC_HEADER_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"

namespace ns3 {

/**
 * \brief 在网计算协议头部
 */
class IncHeader : public Header
{
public:
  // 操作类型定义（1byte)
  enum Operation {
    SUM = 1,           // 求和
    AVERAGE = 2,       // 平均值
    MIN = 3,           // 最小值
    MAX = 4,           // 最大值
    PRODUCT = 5,       // 乘积
    CUSTOM = 6         // 自定义
  };

  // 数据类型定义 (4bit)
  enum DataType {
    INT32 = 1          // 暂只支持32位整数
  };

  // 标志位定义 (4bit)
  enum FlagBits {
    ACK = 0x01,        // 确认
    NACK = 0x02,       // 否定确认
    SYNC = 0x04,       // 连接控制器
    CTRL = 0x08        // 控制器下发配置
  };

  IncHeader();
  virtual ~IncHeader();

  // 必须实现的Header类虚函数
  static TypeId GetTypeId();
  virtual TypeId GetInstanceTypeId() const;
  virtual void Print(std::ostream &os) const;
  virtual void Serialize(Buffer::Iterator start) const;
  virtual uint32_t Deserialize(Buffer::Iterator start);
  virtual uint32_t GetSerializedSize() const;

  // 设置/获取字段
  void SetSrcQP(uint16_t qp);
  uint16_t GetSrcQP() const;
  
  void SetDstQP(uint16_t qp);
  uint16_t GetDstQP() const;
  
  void SetSrcAddr(Ipv4Address addr);
  Ipv4Address GetSrcAddr() const;
  
  void SetDstAddr(Ipv4Address addr); 
  Ipv4Address GetDstAddr() const;
  
  void SetPsn(uint32_t psn);
  uint32_t GetPsn() const;
  
  void SetOperation(Operation op);
  Operation GetOperation() const;
  
  void SetDataType(DataType dataType);
  DataType GetDataType() const;
  
  void SetFlags(uint8_t flags);
  uint8_t GetFlags() const;
  
  void SetCwnd(uint16_t cwnd);
  uint16_t GetCwnd() const;
  
  void SetGroupId(uint16_t groupId);
  uint16_t GetGroupId() const;
  
  void SetLength(uint16_t length);
  uint16_t GetLength() const;
  
  // 添加聚合测试数据字段的getter和setter
  void SetAggDataTest(int32_t value);
  int32_t GetAggDataTest() const;
  
  // 标志位操作
  void SetFlag(FlagBits flag);
  void UnsetFlag(FlagBits flag);
  bool HasFlag(FlagBits flag) const;

private:
  uint16_t m_srcQP;         // 源QP (2 bytes)
  uint16_t m_dstQP;         // 目的QP (2 bytes)
  Ipv4Address m_srcAddr;    // 源地址 (4 bytes)
  Ipv4Address m_dstAddr;    // 目的地址 (4 bytes)
  uint32_t m_psn;           // 序列号 (4 bytes)
  Operation m_operation;    // 操作类型 (1 byte)
  uint8_t m_typeAndFlags;   // 数据类型(4bit) + 标志位(4bit) (1 byte)
  uint16_t m_cwnd;          // 拥塞窗口 (2 bytes)
  uint16_t m_groupId;       // 组ID (2 bytes)
  uint16_t m_length;        // 总长度 (2 bytes)
  int32_t m_aggDataTest;    // 聚合测试数据 (4 bytes)
};

} // namespace ns3

#endif /* INC_HEADER_H */ 