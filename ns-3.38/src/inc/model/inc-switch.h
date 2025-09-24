/*
 * 在网计算协议 - 交换机（网计算引擎）
 */

#ifndef INC_SWITCH_H
#define INC_SWITCH_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/string.h"
#include "ns3/ipv4-address.h"
#include "ns3/socket.h"
#include "ns3/packet.h"
#include "ns3/nstime.h"
#include <map>
#include <vector>
#include <string>
#include "inc-header.h"

namespace ns3
{

/**
 * \brief 在网计算协议交换机（网计算引擎）
 *
 * 实现协议中的交换机功能，包括流分类、数据聚合和结果广播等
 */
class IncSwitch : public Application
{
public:
  // 组状态结构体 - 组内流共享
  struct GroupState {
    uint16_t groupId;          // 组ID
    uint16_t fanIn;            // 扇入度
    uint16_t arraySize;        // 数组长度N
    IncHeader::Operation inc_op;    // 聚合操作类型（默认SUM）
    IncHeader::DataType inc_data_type; // 数据类型（默认INT32）
    uint32_t packet_length;    // 发送报文长度（默认1024字节）
    
    // 缓冲区和状态数组 - 组内共享
    std::vector<int32_t> aggBuffer;      // 聚合缓冲区
    std::vector<uint16_t> degree;        // 聚合度数组
    std::vector<int32_t> bcastBuffer;    // 广播缓冲区
    std::vector<bool> bcastArrState;     // 广播报文抵达数组（每组一个，即下行数据流的报文抵达数组）
    std::vector<uint16_t> rDegree;       // 聚合结果广播度数组
    std::vector<uint32_t> aggPSN;        // 聚合号数组
  };

  /**
   * \brief 获取类型ID
   * \return 对象TypeId
   */
  static TypeId GetTypeId();
  IncSwitch();
  ~IncSwitch() override;

  /**
   * \brief 设置交换机ID
   * \param id 交换机ID
   */
  void SetSwitchId(std::string id);

  /**
   * \brief 获取交换机ID
   * \return 交换机ID
   */
  std::string GetSwitchId() const;

  /**
   * \brief 设置重传超时间隔
   * \param timeout 重传超时时间
   */
  void SetRetransmitTimeout(Time timeout);

  /**
   * \brief 获取重传超时间隔
   * \return 重传超时时间
   */
  Time GetRetransmitTimeout() const;

  /**
   * \brief 初始化引擎配置
   * \param linkState 链接状态信息列表（包含srcIP,srcQP,dstIP,dstQP,to_father_or_son）
   * \param groupId 组ID
   * \param fanIn 扇入度
   * \param arraySize 数组大小
   */
  void InitializeEngine(std::vector<std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, bool>> linkState, 
                        uint16_t groupId, uint16_t fanIn, uint16_t arraySize);

  /**
   * \brief 添加流分类规则，用于流分类表
   * \param srcAddr 源IP地址
   * \param srcQP 源QP
   * \param dstAddr 目的IP地址
   * \param dstQP 目的QP
   * \param isAck 是否是ACK流
   * \param isUpstream 是否是上行流
   */
  void AddFlowClassRule(Ipv4Address srcAddr, uint16_t srcQP, Ipv4Address dstAddr, uint16_t dstQP, bool isAck, bool isUpstream);

  /**
   * \brief 添加入站流上下文，用于入站流上下文表
   * \param srcAddr 源IP地址
   * \param srcQP 源QP
   * \param dstAddr 目的IP地址
   * \param dstQP 目的QP
   * \param fanIn 扇入度
   * \param groupId 组ID
   * \param arraySize 数组大小
   */
  void AddInboundFlowContext(Ipv4Address srcAddr, uint16_t srcQP, Ipv4Address dstAddr, uint16_t dstQP, uint16_t fanIn, uint16_t groupId, uint16_t arraySize);

  /**
   * \brief 添加出站流上下文，用于出站流上下文表  srcAddr和dstAddr指的是入站的方向，即接收报文的方向
   * \param srcAddr 源IP地址
   * \param srcQP 源QP
   * \param dstAddr 目的IP地址
   * \param dstQP 目的QP
   */
  void AddOutboundFlowContext(Ipv4Address srcAddr, uint16_t srcQP, Ipv4Address dstAddr, uint16_t dstQP);

  /**
   * \brief 添加转发规则，用于转换转发表
   * \param srcAddr 源IP地址
   * \param srcQP 源QP
   * \param dstAddr 目的IP地址
   * \param dstQP 目的QP
   * \param nextHopSrcAddr 下一跳源IP地址
   * \param nextHopSrcQP 下一跳源QP
   * \param nextHopDstAddr 下一跳目的IP地址
   * \param nextHopDstQP 下一跳目的QP
   */
  void AddForwardingRule(Ipv4Address srcAddr, uint16_t srcQP, Ipv4Address dstAddr, uint16_t dstQP, 
                          Ipv4Address nextHopSrcAddr, uint16_t nextHopSrcQP, Ipv4Address nextHopDstAddr, uint16_t nextHopDstQP);

  /**
   * \brief 添加组播转发规则（适用于根节点转发或下行数据流转发向多个子节点的情况）
   * \param srcAddr 源IP地址
   * \param srcQP 源QP
   * \param dstAddr 目的IP地址
   * \param dstQP 目的QP
   * \param nextHops 多个下一跳的信息
   */
  void AddMulticastForwardingRule(Ipv4Address srcAddr, uint16_t srcQP, Ipv4Address dstAddr, uint16_t dstQP, 
                                   std::vector<std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t>> nextHops);

  /**
   * \brief 创建组状态
   * \param groupId 组ID
   * \param fanIn 扇入度
   * \param arraySize 数组大小
   * \return 组状态引用
   */
  struct GroupState& CreateGroupState(uint16_t groupId, uint16_t fanIn, uint16_t arraySize);

  /**
   * \brief 获取组状态
   * \param groupId 组ID
   * \return 组状态引用
   */
  struct GroupState& GetGroupState(uint16_t groupId);

  /**
   * \brief 更新聚合号数组AggPSN
   * \param groupId 组ID
   * \param idx 数组索引
   * \param size 步进大小
   */
  void UpdateAggPSN(uint16_t groupId, uint16_t idx, uint16_t size);

  /**
   * \brief 处理重传请求
   * \param packet 收到的数据包
   * \param header 解析出的IncHeader
   */
  void ProcessRetransmission(Ptr<Packet> packet, const IncHeader& header);

  /**
   * \brief 调度重传事件
   * \param header 原始报文的header
   * \param aggDataValue 聚合数据值
   */
  void ScheduleRetransmission(const IncHeader& header, int32_t aggDataValue);

  /**
   * \brief 执行报文重传
   * \param header 原始报文的header
   * \param aggDataValue 聚合数据值
   */
  void RetransmitPacket(const IncHeader& header, int32_t aggDataValue);

  /**
   * \brief 清理组状态
   * \param groupId 组ID
   * \param idx 数组索引
   */
  void ClearGroupState(uint16_t groupId, uint16_t idx);

  // 辅助方法：获取或创建Socket（Socket复用）
  Ptr<Socket> GetOrCreateSocket(Ipv4Address srcAddr, uint16_t srcPort, Ipv4Address dstAddr, uint16_t dstPort);

protected:
  void DoDispose() override;

private:
  void StartApplication() override;
  void StopApplication() override;

  /**
   * \brief 处理数据包接收
   * \param socket 接收数据包的socket
   */
  void HandleRead(Ptr<Socket> socket);

  /**
   * \brief 流分类处理
   * \param packet 收到的数据包
   * \param header 解析出的IncHeader
   * \return 返回流类别，0-未匹配，1-上行数据，2-下行数据，3-上行ACK，4-下行ACK
   */
  uint8_t ClassifyFlow(Ptr<Packet> packet, const IncHeader& header);

  /**
   * \brief 处理上行数据流
   * \param packet 收到的数据包
   * \param header 解析出的IncHeader
   */
  void ProcessUpstreamData(Ptr<Packet> packet, const IncHeader& header);

  /**
   * \brief 处理下行数据流
   * \param packet 收到的数据包
   * \param header 解析出的IncHeader
   */
  void ProcessDownstreamData(Ptr<Packet> packet, const IncHeader& header);

  /**
   * \brief 处理上行ACK流
   * \param packet 收到的数据包
   * \param header 解析出的IncHeader
   */
  void ProcessUpstreamAck(Ptr<Packet> packet, const IncHeader& header);

  /**
   * \brief 处理下行ACK流
   * \param packet 收到的数据包
   * \param header 解析出的IncHeader
   */
  void ProcessDownstreamAck(Ptr<Packet> packet, const IncHeader& header);

  /**
   * \brief 数据聚合流程
   * \param packet 收到的数据包
   * \param header 解析出的IncHeader
   */
  void AggregateData(Ptr<Packet> packet, const IncHeader& header);

  /**
   * \brief 广播数据结果
   * \param packet 要广播的数据包
   * \param header 要广播的头部
   */
  void BroadcastResult(Ptr<Packet> packet, const IncHeader& header);

  /**
   * \brief 发送ACK确认
   * \param header 原始数据包的头部信息
   * \param aggDataTest 聚合测试数据值，默认为0，实际为接收到的报文头部的aggDataTest值
   */
  void SendAck(const IncHeader& header, int32_t aggDataTest = 0);

  /**
   * \brief 发送NAK否定确认
   * \param header 原始数据包的头部信息
   */
  void SendNak(const IncHeader& header);

  
  /**
   * \brief 创建发送数据包的Socket
   * \return 创建的Socket指针
   */

  /*
  Ptr<Socket> CreateSocket();
  */

  uint16_t m_port;       //!< 监听传入数据包的端口
  Ptr<Socket> m_socket;  //!< IPv4 Socket，用于监听接收报文。发送用的socket在入站流上下文查询表和转换转发表中
  Address m_local;       //!< 本地绑定地址
  std::string m_switchId; //!< 交换机ID，用于标识交换机
  Time m_retransmitTimeout; //!< 重传超时间隔

  // Socket缓存：保存已创建的发送Socket，避免重复绑定
  std::map<std::pair<Ipv4Address, uint16_t>, Ptr<Socket>> m_socketCache;
  
  // 流分类表结构
  enum FlowType {
    UNKNOWN_FLOW = 0,
    UPSTREAM_DATA = 1,
    DOWNSTREAM_DATA = 2,
    UPSTREAM_ACK = 3,
    DOWNSTREAM_ACK = 4
  };

  // 带ACK标志的键（用于流分类表和入站流上下文表，key的src和dst指的是入站的方向，即接收报文的方向）
  struct key_with_ack {
    Ipv4Address srcAddr;
    Ipv4Address dstAddr;
    uint16_t dstQP;
    bool isAck;
    
    // 用于map的比较操作
    bool operator<(const key_with_ack& other) const {
      if (srcAddr < other.srcAddr) return true;
      if (other.srcAddr < srcAddr) return false;
      if (dstAddr < other.dstAddr) return true;
      if (other.dstAddr < dstAddr) return false;
      if (dstQP < other.dstQP) return true;
      if (dstQP > other.dstQP) return false;
      return isAck < other.isAck;
    }
  };
  
  // 不带ACK标志的键（用于转换转发表和出站流上下文表，key的src和dst指的是入站的方向，即接收报文的方向）
  struct key_no_ack {
    Ipv4Address srcAddr;
    Ipv4Address dstAddr;
    uint16_t dstQP;
    
    // 用于map的比较操作
    bool operator<(const key_no_ack& other) const {
      if (srcAddr < other.srcAddr) return true;
      if (other.srcAddr < srcAddr) return false;
      if (dstAddr < other.dstAddr) return true;
      if (other.dstAddr < dstAddr) return false;
      return dstQP < other.dstQP;
    }
  };
  
  // 入站流上下文表结构体
  struct InboundFlowContext {
    // 流转换信息（数据流对应的ACK流连接信息，或ACK流对应的数据流连接信息）
    Ipv4Address srcAddr;
    Ipv4Address dstAddr;
    uint16_t srcQP;
    uint16_t dstQP;
    Ptr<Socket> send_Socket;  // 发送用的socket

    // 组信息
    uint16_t groupId;        // 组ID，用于查找组状态
    
    // 流独有的状态数组（不共享）
    std::vector<bool> arrivalState;   // 报文抵达数组（每流一个）
    std::vector<bool> rArrivalState;  // 广播确认报文抵达数组（每流一个）
    
    // 组共享状态的指针
    GroupState* groupStatePtr;        // 指向组状态的指针
  };

  // 下一跳信息
  struct NextHopInfo {
    Ipv4Address srcAddr;
    Ipv4Address dstAddr;
    uint16_t srcQP;
    uint16_t dstQP;
    Ptr<Socket> socket;  // 用于发送到该下一跳的socket
  };

  // 转发规则值
  struct ForwardingValue {
    std::vector<NextHopInfo> nextHops;
  };

  // 出站流上下文表结构体
  struct OutboundFlowContext {
    // 流标识信息  此处src和dst与key的src和dst相反，指的是出站的方向，key是入站的方向
    Ipv4Address srcAddr;
    uint16_t srcQP;
    Ipv4Address dstAddr;
    uint16_t dstQP;
    bool isUpstream;     // 是否是上行流
    
    // 重传信息
    int32_t* bufferPtr;  // 指向发送缓冲区的指针(aggBuffer或bcastBuffer)
    std::map<uint32_t, EventId> retransmitEvents;  // PSN -> 重传事件ID
    std::map<uint32_t, int32_t> retransmitValues;  // PSN -> agg_data_test值
  };

  // 跟踪回调
  TracedCallback<Ptr<const Packet>> m_rxTrace;
  TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_rxTraceWithAddresses;

  // 表和状态存储
  std::map<key_with_ack, FlowType> m_flowClassTable;                // 流分类表
  std::map<key_no_ack, InboundFlowContext> m_inboundFlowContextTable; // 入站流上下文表
  std::map<key_with_ack, ForwardingValue> m_forwardingTable;       // 转换转发表
  std::map<key_no_ack, OutboundFlowContext> m_outboundFlowContextTable; // 出站流上下文表，更准确的作用是计时重传表
  std::map<uint16_t, GroupState> m_groupStateTable;               // 组状态表（按组ID索引）
  
};

} // namespace ns3

#endif /* INC_SWITCH_H */ 