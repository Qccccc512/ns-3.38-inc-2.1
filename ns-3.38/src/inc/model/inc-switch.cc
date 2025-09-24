/*
 * 在网计算协议 - 交换机（网计算引擎）实现
 */

#include "inc-switch.h"
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "inc-header.h"
#include <cstdlib>
#include <iostream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("IncSwitch");

NS_OBJECT_ENSURE_REGISTERED(IncSwitch);

TypeId
IncSwitch::GetTypeId()
{
  static TypeId tid =
      TypeId("ns3::IncSwitch")
          .SetParent<Application>()
          .SetGroupName("Applications")
          .AddConstructor<IncSwitch>()
          .AddAttribute("Port",
                      "监听端口",
                      UintegerValue(9),
                      MakeUintegerAccessor(&IncSwitch::m_port),
                      MakeUintegerChecker<uint16_t>())
          .AddAttribute("SwitchId",
                      "交换机标识符",
                      StringValue(""),
                      MakeStringAccessor(&IncSwitch::m_switchId),
                      MakeStringChecker())
          .AddAttribute("RetransmitTimeout",
                      "重传超时间隔",
                      TimeValue(MilliSeconds(20)),
                      MakeTimeAccessor(&IncSwitch::m_retransmitTimeout),
                      MakeTimeChecker())
          .AddTraceSource("Rx",
                        "接收数据包",
                        MakeTraceSourceAccessor(&IncSwitch::m_rxTrace),
                        "ns3::Packet::TracedCallback")
          .AddTraceSource("RxWithAddresses",
                        "接收数据包，包含地址信息",
                        MakeTraceSourceAccessor(&IncSwitch::m_rxTraceWithAddresses),
                        "ns3::Packet::TwoAddressTracedCallback");
  return tid;
}

IncSwitch::IncSwitch()
    : m_port(9),
      m_socket(nullptr),
      m_switchId(""),
      m_retransmitTimeout(MilliSeconds(10))
{
  NS_LOG_FUNCTION(this);
}

IncSwitch::~IncSwitch()
{
  NS_LOG_FUNCTION(this);
}

void
IncSwitch::SetSwitchId(std::string id)
{
  NS_LOG_FUNCTION(this << id);
  m_switchId = id;
}

std::string
IncSwitch::GetSwitchId() const
{
  return m_switchId;
}

void
IncSwitch::SetRetransmitTimeout(Time timeout)
{
  NS_LOG_FUNCTION(this << timeout);
  m_retransmitTimeout = timeout;
}

Time
IncSwitch::GetRetransmitTimeout() const
{
  return m_retransmitTimeout;
}

void
IncSwitch::DoDispose()
{
  NS_LOG_FUNCTION(this);
  
  if (m_socket != nullptr)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
  
  // 清理Socket缓存
  for (auto& socketPair : m_socketCache)
  {
    if (socketPair.second != nullptr)
    {
      socketPair.second->Close();
      socketPair.second = nullptr;
    }
  }
  m_socketCache.clear();
  
  // 取消所有重传事件
  for (auto& ctx : m_outboundFlowContextTable)
  {
    for (auto& event : ctx.second.retransmitEvents)
    {
      if (event.second.IsRunning())
      {
        event.second.Cancel();
      }
    }
  }
  
  // 清空表和状态
  m_flowClassTable.clear();
  m_inboundFlowContextTable.clear();
  m_forwardingTable.clear();
  m_outboundFlowContextTable.clear();
  m_groupStateTable.clear();
  
  Application::DoDispose();
}

// 引擎初始化方法
void
IncSwitch::InitializeEngine(std::vector<std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, bool>> linkState,
                           uint16_t groupId, uint16_t fanIn, uint16_t arraySize)
{
  NS_LOG_FUNCTION(this << groupId << fanIn << arraySize);
  
  NS_LOG_INFO(m_switchId << " 初始化引擎: 组ID=" << groupId << " 扇入度=" << fanIn << " 数组大小=" << arraySize);
  
  // 创建组状态
  CreateGroupState(groupId, fanIn, arraySize);
  
  // 检查是否有到父节点的链路
  bool hasLinkToFather = false;
  for (const auto& link : linkState) {
    if (!std::get<4>(link)) { // to_father_or_son=false表示到父节点的链路
      hasLinkToFather = true;
      break;
    }
  }
  
  NS_LOG_INFO(m_switchId << " 是否有到父节点链路: " << (hasLinkToFather ? "是" : "否"));
  
  // 配置各个查询表
  for (const auto& link : linkState) {
    Ipv4Address srcAddr = std::get<0>(link);  // 交换机向外发送的源IP
    uint16_t srcQP = std::get<1>(link);       // 交换机向外发送的源QP
    Ipv4Address dstAddr = std::get<2>(link);  // 交换机向外发送的目的IP
    uint16_t dstQP = std::get<3>(link);       // 交换机向外发送的目的QP
    bool toSon = std::get<4>(link);           // 是否是到子节点的链路
    
    // 查询表中使用的是入站方向，因此需要反转源/目的地址
    if (toSon) {
      // 到子节点的链路：配置子节点->交换机的上行数据流和ACK流
      AddFlowClassRule(dstAddr, dstQP, srcAddr, srcQP, false, true);  // 上行数据流
      AddFlowClassRule(dstAddr, dstQP, srcAddr, srcQP, true, true);   // 上行ACK流
      
      // 添加入站流上下文
      AddInboundFlowContext(dstAddr, dstQP, srcAddr, srcQP, fanIn, groupId, arraySize);
      
      // 添加出站流上下文
      AddOutboundFlowContext(srcAddr, srcQP, dstAddr, dstQP);
      
      // 配置转发规则（非根节点：子节点的上行数据流转发给父节点）
      if (hasLinkToFather) {
        // 找到父节点链路
        for (const auto& fatherLink : linkState) {
          if (!std::get<4>(fatherLink)) { // 这是到父节点的链路
            Ipv4Address fatherSrcAddr = std::get<0>(fatherLink);
            uint16_t fatherSrcQP = std::get<1>(fatherLink);
            Ipv4Address fatherDstAddr = std::get<2>(fatherLink);
            uint16_t fatherDstQP = std::get<3>(fatherLink);
            
            // 添加从子节点到父节点的转发规则
            AddForwardingRule(dstAddr, dstQP, srcAddr, srcQP, 
                              fatherSrcAddr, fatherSrcQP, fatherDstAddr, fatherDstQP);
            break;
          }
        }
      } 
      else {
        // 根节点：子节点的上行数据流转发给所有子节点（包括原发送节点）
        std::vector<std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t>> multicastHops;
        
        for (const auto& sonLink : linkState) {
          Ipv4Address sonSrcAddr = std::get<0>(sonLink);
          uint16_t sonSrcQP = std::get<1>(sonLink);
          Ipv4Address sonDstAddr = std::get<2>(sonLink);
          uint16_t sonDstQP = std::get<3>(sonLink);
          
          multicastHops.push_back(std::make_tuple(sonSrcAddr, sonSrcQP, sonDstAddr, sonDstQP));
        }
        
        // 添加转发规则
        AddMulticastForwardingRule(dstAddr, dstQP, srcAddr, srcQP, multicastHops);
      }
    } 
    else {
      // 到父节点的链路：配置父节点->交换机的下行数据流和ACK流
      AddFlowClassRule(dstAddr, dstQP, srcAddr, srcQP, false, false);  // 下行数据流
      AddFlowClassRule(dstAddr, dstQP, srcAddr, srcQP, true, false);   // 下行ACK流
      
      // 添加入站流上下文
      AddInboundFlowContext(dstAddr, dstQP, srcAddr, srcQP, fanIn, groupId, arraySize);
      
      // 添加出站流上下文
      AddOutboundFlowContext(srcAddr, srcQP, dstAddr, dstQP);
      
      // 配置转发规则：父节点的下行数据流转发给所有子节点
      std::vector<std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t>> multicastHops;
      
      for (const auto& sonLink : linkState) {
        if (std::get<4>(sonLink)) { // 这是到子节点的链路
          Ipv4Address sonSrcAddr = std::get<0>(sonLink);
          uint16_t sonSrcQP = std::get<1>(sonLink);
          Ipv4Address sonDstAddr = std::get<2>(sonLink);
          uint16_t sonDstQP = std::get<3>(sonLink);
          
          multicastHops.push_back(std::make_tuple(sonSrcAddr, sonSrcQP, sonDstAddr, sonDstQP));
        }
      }
      
      // 添加转发规则
      AddMulticastForwardingRule(dstAddr, dstQP, srcAddr, srcQP, multicastHops);
    }
  }
  
  NS_LOG_INFO(m_switchId << " 引擎初始化完成");
}

// 处理数据包接收
void
IncSwitch::HandleRead(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this << socket);
  
  Ptr<Packet> packet;
  Address from;
  Address localAddress;

  while ((packet = socket->RecvFrom(from)))
  {
    socket->GetSockName(localAddress);
    
    // 记录跟踪信息
    m_rxTrace(packet);
    m_rxTraceWithAddresses(packet, from, localAddress);

    /*if (InetSocketAddress::IsMatchingType(from))
    {
      NS_LOG_INFO(m_switchId << " 接收到数据包 大小=" << packet->GetSize() 
                  << " 字节 来自=" << InetSocketAddress::ConvertFrom(from).GetIpv4() 
                  << ":" << InetSocketAddress::ConvertFrom(from).GetPort());
    }*/
    
    // 解析INC头部
    IncHeader header;
    Ptr<Packet> packetCopy = packet->Copy(); // 创建副本以保持原始报文
    packetCopy->RemoveHeader(header);
    
    // 显示头部信息
    /*NS_LOG_INFO(m_switchId << " 报文头部: src=" << header.GetSrcAddr() 
                << ":" << header.GetSrcQP() << " dst=" << header.GetDstAddr() 
                << ":" << header.GetDstQP() << " PSN=" << header.GetPsn() 
                << " ACK=" << header.HasFlag(IncHeader::ACK));*/
    
    // 流分类
    uint8_t flowType = ClassifyFlow(packet, header);
    
    // 根据流类型处理
    switch (flowType)
    {
      case UPSTREAM_DATA:
        /*NS_LOG_INFO(m_switchId << " 处理上行数据流 PSN=" << header.GetPsn());*/
        ProcessUpstreamData(packetCopy, header);
        break;
      
      case DOWNSTREAM_DATA:
        //NS_LOG_INFO(m_switchId << " 处理下行数据流 PSN=" << header.GetPsn());
        ProcessDownstreamData(packetCopy, header);
        break;
      
      case UPSTREAM_ACK:
        //NS_LOG_INFO(m_switchId << " 处理上行ACK PSN=" << header.GetPsn());
        ProcessUpstreamAck(packetCopy, header);
        break;
      
      case DOWNSTREAM_ACK:
        //NS_LOG_INFO(m_switchId << " 处理下行ACK PSN=" << header.GetPsn());
        ProcessDownstreamAck(packetCopy, header);
        break;
      
      default:
        NS_LOG_INFO(m_switchId << " 未知流类型，忽略报文");
        break;
    }
  }
}

// 流分类处理
uint8_t
IncSwitch::ClassifyFlow(Ptr<Packet> packet, const IncHeader& header)
{
  NS_LOG_FUNCTION(this);
  
  // 获取报文首部信息
  Ipv4Address srcAddr = header.GetSrcAddr();
  Ipv4Address dstAddr = header.GetDstAddr();
  uint16_t dstQP = header.GetDstQP();
  bool isAck = header.HasFlag(IncHeader::ACK) || header.HasFlag(IncHeader::NACK);
  
  // 创建查询键
  key_with_ack key;
  key.srcAddr = srcAddr;
  key.dstAddr = dstAddr;
  key.dstQP = dstQP;
  key.isAck = isAck;
  
  // 查询流分类表
  auto it = m_flowClassTable.find(key);
  if (it != m_flowClassTable.end())
  {
    FlowType flowType = it->second;
    /*NS_LOG_INFO(m_switchId << " 流分类: src=" << srcAddr 
                << " dst=" << dstAddr << " dstQP=" << dstQP 
                << " isAck=" << isAck << " -> FlowType=" << static_cast<int>(flowType));*/
    return flowType;
  }
  
  NS_LOG_INFO(m_switchId << " 未匹配流分类: src=" << srcAddr 
              << " dst=" << dstAddr << " dstQP=" << dstQP 
              << " isAck=" << isAck);
  return UNKNOWN_FLOW;
}


// 添加流分类规则
void
IncSwitch::AddFlowClassRule(Ipv4Address srcAddr, uint16_t srcQP, Ipv4Address dstAddr, uint16_t dstQP, bool isAck, bool isUpstream)
{
  NS_LOG_FUNCTION(this << srcAddr << srcQP << dstAddr << dstQP << isAck << isUpstream);
  
  key_with_ack key;
  key.srcAddr = srcAddr;
  key.dstAddr = dstAddr;
  key.dstQP = dstQP;
  key.isAck = isAck;
  
  // 根据isAck和isUpstream设置流类型
  FlowType flowType;
  if (isAck) {
    flowType = isUpstream ? UPSTREAM_ACK : DOWNSTREAM_ACK;
  } else {
    flowType = isUpstream ? UPSTREAM_DATA : DOWNSTREAM_DATA;
  }
  
  // 添加到流分类表
  m_flowClassTable[key] = flowType;
  
  NS_LOG_INFO(m_switchId << " 添加流分类规则: " << srcAddr << ":" << srcQP << " -> " 
              << dstAddr << ":" << dstQP << " IsAck=" << isAck 
              << " IsUpstream=" << isUpstream << " FlowType=" << static_cast<int>(flowType));
}

// 添加入站流上下文
void
IncSwitch::AddInboundFlowContext(Ipv4Address srcAddr, uint16_t srcQP, Ipv4Address dstAddr, uint16_t dstQP, 
                                uint16_t fanIn, uint16_t groupId, uint16_t arraySize)
{ 
  NS_LOG_FUNCTION(this << srcAddr << srcQP << dstAddr << dstQP << fanIn << groupId << arraySize);
  
  // 创建键
  key_no_ack key;
  key.srcAddr = srcAddr;
  key.dstAddr = dstAddr;
  key.dstQP = dstQP;
  
  // 创建或获取组状态
  GroupState& groupState = CreateGroupState(groupId, fanIn, arraySize);
  
  // 创建入站流上下文
  InboundFlowContext context;
  // 出站与入站方向是反的
  context.srcAddr = dstAddr;
  context.dstAddr = srcAddr;
  context.srcQP = dstQP;
  context.dstQP = srcQP;
  context.groupId = groupId;
  context.groupStatePtr = &groupState;
  
  // 初始化流独有的状态数组
  context.arrivalState.resize(arraySize, false);
  context.rArrivalState.resize(arraySize, false);
  
  // 使用复用机制获取或创建Socket
  uint16_t srcPort = dstQP + 1024;
  context.send_Socket = GetOrCreateSocket(dstAddr, srcPort, srcAddr, 9);
  
  // 添加到入站流上下文表
  m_inboundFlowContextTable[key] = context;
  
  NS_LOG_INFO(m_switchId << " 添加入站流上下文: " << srcAddr << ":" << srcQP 
              << " -> " << dstAddr << ":" << dstQP 
              << " 组ID=" << groupId << " 扇入度=" << fanIn 
              << " 数组大小=" << arraySize);
}

// 添加出站流上下文
void
IncSwitch::AddOutboundFlowContext(Ipv4Address srcAddr, uint16_t srcQP, Ipv4Address dstAddr, uint16_t dstQP)
{
  NS_LOG_FUNCTION(this << srcAddr << srcQP << dstAddr << dstQP);
  
  // 创建键，此处无需修改，因为参数中的src/dst指出站方向，键中的src/dst是入站方向，此处需要反向
  key_no_ack key;
  key.srcAddr = dstAddr; // 入站方向是反的
  key.dstAddr = srcAddr;
  key.dstQP = srcQP;
  
  // 创建出站流上下文
  OutboundFlowContext context;
  context.srcAddr = srcAddr;
  context.srcQP = srcQP;
  context.dstAddr = dstAddr;
  context.dstQP = dstQP;

  //此处实际上并未使用
  context.isUpstream = false; // 默认为下行流，在InitializeEngine中会根据to_father_or_son设置
  context.bufferPtr = nullptr;
  
  // 添加到出站流上下文表
  m_outboundFlowContextTable[key] = context;
  
  NS_LOG_INFO(m_switchId << " 添加出站流上下文: " << srcAddr << ":" << srcQP 
              << " -> " << dstAddr << ":" << dstQP);
}

// 添加转发规则
void
IncSwitch::AddForwardingRule(Ipv4Address srcAddr, uint16_t srcQP, Ipv4Address dstAddr, uint16_t dstQP,
                            Ipv4Address nextHopSrcAddr, uint16_t nextHopSrcQP, Ipv4Address nextHopDstAddr, uint16_t nextHopDstQP)
{
  NS_LOG_FUNCTION(this << srcAddr << srcQP << dstAddr << dstQP 
                  << nextHopSrcAddr << nextHopSrcQP << nextHopDstAddr << nextHopDstQP);
  
  // 创建键
  key_with_ack key;
  key.srcAddr = srcAddr;
  key.dstAddr = dstAddr;
  key.dstQP = dstQP;
  key.isAck = false; // 数据流，非ACK
  
  // 创建下一跳信息
  NextHopInfo nextHop;
  nextHop.srcAddr = nextHopSrcAddr;
  nextHop.srcQP = nextHopSrcQP;
  nextHop.dstAddr = nextHopDstAddr;
  nextHop.dstQP = nextHopDstQP;
  
  // 获取或创建Socket
  uint16_t srcPort = nextHopSrcQP + 1024;
  nextHop.socket = GetOrCreateSocket(nextHopSrcAddr, srcPort, nextHopDstAddr, 9);
  
  // 查找是否已存在转发规则
  auto it = m_forwardingTable.find(key);
  if (it == m_forwardingTable.end()) {
    // 创建新的转发规则
    ForwardingValue value;
    value.nextHops.push_back(nextHop);
    m_forwardingTable[key] = value;
  } else {
    // 添加到现有转发规则
    it->second.nextHops.push_back(nextHop);
  }
  
  NS_LOG_INFO(m_switchId << " 添加转发规则: " << srcAddr << ":" << srcQP 
              << " -> " << dstAddr << ":" << dstQP 
              << " 下一跳: " << nextHopSrcAddr << ":" << nextHopSrcQP 
              << " -> " << nextHopDstAddr << ":" << nextHopDstQP);
}

// 添加转发规则
void
IncSwitch::AddMulticastForwardingRule(Ipv4Address srcAddr, uint16_t srcQP, Ipv4Address dstAddr, uint16_t dstQP,
                                     std::vector<std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t>> nextHops)
{
  NS_LOG_FUNCTION(this << srcAddr << srcQP << dstAddr << dstQP);
  
  // 创建键
  key_with_ack key;
  key.srcAddr = srcAddr;
  key.dstAddr = dstAddr;
  key.dstQP = dstQP;
  key.isAck = false; // 数据流，非ACK
  
  // 创建转发规则
  ForwardingValue value;
  
  // 添加所有下一跳
  for (const auto& hop : nextHops) {
    NextHopInfo nextHop;
    nextHop.srcAddr = std::get<0>(hop);
    nextHop.srcQP = std::get<1>(hop);
    nextHop.dstAddr = std::get<2>(hop);
    nextHop.dstQP = std::get<3>(hop);
    
    // 获取或创建Socket
    uint16_t srcPort = nextHop.srcQP + 1024;
    nextHop.socket = GetOrCreateSocket(nextHop.srcAddr, srcPort, nextHop.dstAddr, 9);
    
    value.nextHops.push_back(nextHop);
    
    NS_LOG_INFO(m_switchId << " 添加组播下一跳: " << nextHop.srcAddr << ":" << nextHop.srcQP 
                << " -> " << nextHop.dstAddr << ":" << nextHop.dstQP);
  }
  
  // 设置转发规则
  m_forwardingTable[key] = value;
  
  NS_LOG_INFO(m_switchId << " 添加组播转发规则: " << srcAddr << ":" << srcQP 
              << " -> " << dstAddr << ":" << dstQP 
              << " 下一跳数量: " << nextHops.size());
}

// 创建组状态
struct IncSwitch::GroupState&
IncSwitch::CreateGroupState(uint16_t groupId, uint16_t fanIn, uint16_t arraySize)
{
  NS_LOG_FUNCTION(this << groupId << fanIn << arraySize);
  
  // 检查组ID是否已存在
  auto it = m_groupStateTable.find(groupId);
  if (it != m_groupStateTable.end()) {
    NS_LOG_INFO(m_switchId << " 组ID已存在: " << groupId);
    return it->second;
  }
  
    // 创建新组
    GroupState newGroup;
    newGroup.groupId = groupId;
    newGroup.fanIn = fanIn;
  newGroup.arraySize = arraySize;
  newGroup.inc_op = IncHeader::SUM; // 默认聚合操作为SUM
  newGroup.inc_data_type = IncHeader::INT32; // 默认数据类型为INT32
  newGroup.packet_length = 1024; // 默认发送报文长度为1KB
    
    // 初始化各个数组
  newGroup.aggBuffer.resize(arraySize, 0);
  newGroup.degree.resize(arraySize, 0);
  newGroup.bcastBuffer.resize(arraySize, 0);
  newGroup.bcastArrState.resize(arraySize, false);
  newGroup.rDegree.resize(arraySize, 0);
  
  // 正确初始化aggPSN数组，每个元素值为其索引
  newGroup.aggPSN.resize(arraySize);
  for (uint16_t i = 0; i < arraySize; ++i) {
      newGroup.aggPSN[i] = i;
    }
    
  // 将新组加入组状态表
    m_groupStateTable[groupId] = newGroup;
    
  NS_LOG_INFO(m_switchId << " 创建组: " << groupId 
              << " 扇入度=" << fanIn << " 数组大小=" << arraySize);
  
  return m_groupStateTable[groupId];
}

// 获取组状态
struct IncSwitch::GroupState&
IncSwitch::GetGroupState(uint16_t groupId)
{
  NS_LOG_FUNCTION(this << groupId);
  
  auto it = m_groupStateTable.find(groupId);
  if (it == m_groupStateTable.end()) {
    NS_FATAL_ERROR("组ID不存在: " << groupId);
  }
  
  return it->second;
}

// 更新聚合号数组
void
IncSwitch::UpdateAggPSN(uint16_t groupId, uint16_t idx, uint16_t size)
{
  NS_LOG_FUNCTION(this << groupId << idx << size);
  
  auto it = m_groupStateTable.find(groupId);
  if (it == m_groupStateTable.end()) {
    NS_LOG_ERROR(m_switchId << " 组ID不存在: " << groupId);
    return;
  }
  
  // 更新AggPSN
  it->second.aggPSN[idx] += size;
  
  // 清除所有相关流的arrivalState
  for (auto& flowPair : m_inboundFlowContextTable) {
    InboundFlowContext& flowContext = flowPair.second;
    if (flowContext.groupId == groupId) {
      // 确保索引在范围内
      if (idx < flowContext.arrivalState.size()) {
        flowContext.arrivalState[idx] = false;
      }
    }
  }
  
  NS_LOG_INFO(m_switchId << " 更新AggPSN: 组ID=" << groupId 
              << " 索引=" << idx << " 新值=" << it->second.aggPSN[idx]);
}

// 清理组状态
void
IncSwitch::ClearGroupState(uint16_t groupId, uint16_t idx)
{
  NS_LOG_FUNCTION(this << groupId << idx);
  
  auto it = m_groupStateTable.find(groupId);
  if (it == m_groupStateTable.end()) {
    NS_LOG_ERROR(m_switchId << " 组ID不存在: " << groupId);
    return;
  }
  
  GroupState& group = it->second;
  
  // 清空状态
  group.aggBuffer[idx] = 0;
  group.degree[idx] = 0;
  group.bcastArrState[idx] = false;
  group.rDegree[idx] = 0;
  group.bcastBuffer[idx] = 0;
  
  
  // 查找并清理所有使用该组状态的流上下文中的对应标志
  for (auto& flowPair : m_inboundFlowContextTable) {
    InboundFlowContext& flowContext = flowPair.second;
    if (flowContext.groupId == groupId) {
      flowContext.arrivalState[idx] = false;
      flowContext.rArrivalState[idx] = false;
    }
  }
  
  NS_LOG_INFO(m_switchId << " 清理组状态: 组ID=" << groupId << " 索引=" << idx);
}


void
IncSwitch::StartApplication()
{
  NS_LOG_FUNCTION(this);

  if (m_socket == nullptr)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    if (m_socket->Bind(local) == -1)
    {
      NS_FATAL_ERROR("绑定失败: " << m_socket->GetErrno());
    }
  }

  m_socket->SetRecvCallback(MakeCallback(&IncSwitch::HandleRead, this));

  if (m_socket->GetSocketType() != Socket::NS3_SOCK_DGRAM)
  {
    NS_LOG_INFO("非UDP Socket，关闭");
    StopApplication();
    return;
  }
  
  NS_LOG_INFO(m_switchId << " 启动成功，监听端口: " << m_port);
}

void
IncSwitch::StopApplication()
{
  NS_LOG_FUNCTION(this);
  
  if (m_socket != nullptr)
  {
    m_socket->Close();
    m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
  }
  
  // 清理Socket缓存
  for (auto& socketPair : m_socketCache)
  {
    if (socketPair.second != nullptr)
    {
      socketPair.second->Close();
    }
  }
  m_socketCache.clear();
  
  // 取消所有重传事件
  for (auto& ctx : m_outboundFlowContextTable)
  {
    for (auto& event : ctx.second.retransmitEvents)
    {
      if (event.second.IsRunning())
      {
        event.second.Cancel();
      }
    }
  }
  
  // 清空表和状态
  m_flowClassTable.clear();
  m_inboundFlowContextTable.clear();
  m_forwardingTable.clear();
  m_outboundFlowContextTable.clear();
  m_groupStateTable.clear();
  
  NS_LOG_INFO(m_switchId << " 停止应用程序，已清理所有状态和事件");
}

// 处理上行数据流
void
IncSwitch::ProcessUpstreamData(Ptr<Packet> packet, const IncHeader& header)
{
  NS_LOG_FUNCTION(this);
  
  // 获取关键信息
  Ipv4Address srcAddr = header.GetSrcAddr();
  Ipv4Address dstAddr = header.GetDstAddr();
  uint16_t dstQP = header.GetDstQP();
  uint32_t psn = header.GetPsn();
  int32_t aggDataTest = header.GetAggDataTest();
  
  NS_LOG_INFO(m_switchId << " 处理上行数据流: src=" << srcAddr 
              << " dst=" << dstAddr << " dstQP=" << dstQP 
              << " PSN=" << psn << " aggDataTest=" << aggDataTest);
  
  // 查找入站流上下文
  key_no_ack key;
  key.srcAddr = srcAddr;
  key.dstAddr = dstAddr;
  key.dstQP = dstQP;
  
  auto contextIt = m_inboundFlowContextTable.find(key);
  if (contextIt == m_inboundFlowContextTable.end()) {
    NS_LOG_ERROR(m_switchId << " 未找到入站流上下文，丢弃上行数据: " 
                << srcAddr << "->" << dstAddr << ":" << dstQP);
    return;
  }
  
  // 获取上下文和组状态
  InboundFlowContext& context = contextIt->second;
  GroupState* groupState = context.groupStatePtr;
  
  if (!groupState) {
    NS_LOG_ERROR(m_switchId << " 组状态指针为空，丢弃上行数据");
    return;
  }
  
  // 计算索引
  uint16_t idx = psn % groupState->arraySize;
  
  // 顺序性检测：检查PSN和AggPSN[idx]的关系
  if (psn < groupState->aggPSN[idx]) {
    // 滞后情况：发送ACK并丢弃数据
    NS_LOG_INFO(m_switchId << " 上行数据滞后: PSN=" << psn 
                << " AggPSN=" << groupState->aggPSN[idx]);
    SendAck(header, aggDataTest);
    return;
  } 
  else if (psn > groupState->aggPSN[idx]) {
    // 超前情况：将报文交给重传模块
    NS_LOG_INFO(m_switchId << " 上行数据超前: PSN=" << psn 
                << " AggPSN=" << groupState->aggPSN[idx]);
    ProcessRetransmission(packet, header);
    return;
  }
  
  // PSN=AggPSN[idx]的持平情况，进行冗余性检测
  if (context.arrivalState[idx] || groupState->bcastArrState[idx]) {
    // 重传情况：发送ACK并将报文交给重传模块
    NS_LOG_INFO(m_switchId << " 上行数据重传: PSN=" << psn<<"arrivalState "<< context.arrivalState[idx] <<"bcastArrState "<< groupState->bcastArrState[idx]);
    SendAck(header, aggDataTest);
    ProcessRetransmission(packet, header);
    return;
  }
  
  // 首传情况：发送ACK，更新状态，将报文交给聚合模块
  NS_LOG_INFO(m_switchId << " 上行数据首传: PSN=" << psn);
  SendAck(header, aggDataTest);
  
  // 更新状态
  context.arrivalState[idx] = true;
  context.rArrivalState[idx] = false;
  
  // 将数据报文交给聚合模块
  AggregateData(packet, header);
}

// 处理下行数据流
void
IncSwitch::ProcessDownstreamData(Ptr<Packet> packet, const IncHeader& header)
{
  NS_LOG_FUNCTION(this);
  
  // 获取关键信息
  Ipv4Address srcAddr = header.GetSrcAddr();
  Ipv4Address dstAddr = header.GetDstAddr();
  uint16_t dstQP = header.GetDstQP();
  uint32_t psn = header.GetPsn();
  int32_t aggDataTest = header.GetAggDataTest();
  
  NS_LOG_INFO(m_switchId << " 处理下行数据流: src=" << srcAddr 
              << " dst=" << dstAddr << " dstQP=" << dstQP 
              << " PSN=" << psn << " aggDataTest=" << aggDataTest);
  
  // 查找入站流上下文
  key_no_ack key;
  key.srcAddr = srcAddr;
  key.dstAddr = dstAddr;
  key.dstQP = dstQP;
  
  auto contextIt = m_inboundFlowContextTable.find(key);
  if (contextIt == m_inboundFlowContextTable.end()) {
    NS_LOG_ERROR(m_switchId << " 未找到入站流上下文，丢弃下行数据: " 
                << srcAddr << "->" << dstAddr << ":" << dstQP);
    return;
  }
  
  // 获取上下文和组状态
  InboundFlowContext& context = contextIt->second;
  GroupState* groupState = context.groupStatePtr;
  
  if (!groupState) {
    NS_LOG_ERROR(m_switchId << " 组状态指针为空，丢弃下行数据");
    return;
  }
  
  // 计算索引
  uint16_t idx = psn % groupState->arraySize;
  
  // 顺序性检测：下行数据流不会有超前情况，只检查滞后
  if (psn < groupState->aggPSN[idx]) {
    // 滞后情况：发送ACK并丢弃数据
    NS_LOG_INFO(m_switchId << " 下行数据滞后: PSN=" << psn 
                << " AggPSN=" << groupState->aggPSN[idx]);
    SendAck(header, aggDataTest);
    return;
  }
  
  // 冗余检测，检查广播报文抵达状态
  if (groupState->bcastArrState[idx]) {
    // 重传情况：发送ACK并丢弃报文
    NS_LOG_INFO(m_switchId << " 下行数据重传: PSN=" << psn);
    SendAck(header, aggDataTest);
    return;
  }
  
  // 首传情况：发送ACK，更新状态，将报文缓存并广播
  NS_LOG_INFO(m_switchId << " 下行数据首传: PSN=" << psn);
  SendAck(header, aggDataTest);
  
  // 更新状态
  groupState->bcastArrState[idx] = true;
  
  // 缓存聚合值到广播缓冲区
  groupState->bcastBuffer[idx] = aggDataTest;
  
  NS_LOG_INFO(m_switchId << " 缓存下行数据到广播缓冲区: PSN=" << psn 
              << " 值=" << aggDataTest);
  
  // 将报文交给广播模块
  BroadcastResult(packet, header);
}

// 数据聚合流程
void
IncSwitch::AggregateData(Ptr<Packet> packet, const IncHeader& header)
{
  NS_LOG_FUNCTION(this);
  
  // 获取关键信息
  Ipv4Address srcAddr = header.GetSrcAddr();
  Ipv4Address dstAddr = header.GetDstAddr();
  uint16_t dstQP = header.GetDstQP();
  uint32_t psn = header.GetPsn();
  int32_t aggDataTest = header.GetAggDataTest();
  
  // 查找入站流上下文
  key_no_ack key;
  key.srcAddr = srcAddr;
  key.dstAddr = dstAddr;
  key.dstQP = dstQP;
  
  auto contextIt = m_inboundFlowContextTable.find(key);
  if (contextIt == m_inboundFlowContextTable.end()) {
    NS_LOG_ERROR(m_switchId << " 未找到入站流上下文，无法聚合数据");
    return;
  }
  
  // 获取上下文和组状态
  InboundFlowContext& context = contextIt->second;
  GroupState* groupState = context.groupStatePtr;
  
  if (!groupState) {
    NS_LOG_ERROR(m_switchId << " 组状态指针为空，无法聚合数据");
    return;
  }
  
  // 计算索引
  uint16_t idx = psn % groupState->arraySize;
  
  // 聚合操作
  IncHeader::Operation op = groupState->inc_op;
  
  // 写阶段：执行聚合操作
  switch (op) {
    case IncHeader::SUM:
      groupState->aggBuffer[idx] += aggDataTest;
      break;
    case IncHeader::AVERAGE:
      // 暂存和，后续除以计数
      groupState->aggBuffer[idx] += aggDataTest;
      break;
    case IncHeader::MIN:
      if (groupState->degree[idx] == 0 || aggDataTest < groupState->aggBuffer[idx]) {
        groupState->aggBuffer[idx] = aggDataTest;
      }
      break;
    case IncHeader::MAX:
      if (groupState->degree[idx] == 0 || aggDataTest > groupState->aggBuffer[idx]) {
        groupState->aggBuffer[idx] = aggDataTest;
      }
      break;
    case IncHeader::PRODUCT:
      if (groupState->degree[idx] == 0) {
        groupState->aggBuffer[idx] = aggDataTest;
      } else {
        groupState->aggBuffer[idx] *= aggDataTest;
      }
      break;
    default:
      // 默认使用求和
      groupState->aggBuffer[idx] += aggDataTest;
      break;
  }
  
  // 更新聚合度
  groupState->degree[idx]++;
  
  NS_LOG_INFO(m_switchId << " 聚合数据: PSN=" << psn 
              << " 新值=" << aggDataTest 
              << " 聚合结果=" << groupState->aggBuffer[idx] 
              << " 聚合度=" << groupState->degree[idx] 
              << "/" << groupState->fanIn);
  
  // 读阶段：检查聚合度是否达到扇入度
  if (groupState->degree[idx] == groupState->fanIn) {
    // 如果是AVERAGE操作，执行除法计算均值
    if (op == IncHeader::AVERAGE) {
      groupState->aggBuffer[idx] /= groupState->fanIn;
    }
    
    NS_LOG_INFO(m_switchId << " 聚合完成，准备转发: PSN=" << psn 
                << " 聚合结果=" << groupState->aggBuffer[idx]);
    
    // 查找转发规则
    key_with_ack forwardKey;
    forwardKey.srcAddr = srcAddr;
    forwardKey.dstAddr = dstAddr;
    forwardKey.dstQP = dstQP;
    forwardKey.isAck = false;
    
    auto forwardIt = m_forwardingTable.find(forwardKey);
    if (forwardIt == m_forwardingTable.end()) {
      NS_LOG_ERROR(m_switchId << " 未找到转发规则，无法转发聚合结果: " 
                  << srcAddr << "->" << dstAddr << ":" << dstQP);
      return;
    }
    
    const ForwardingValue& forwardValue = forwardIt->second;
    
    // 判断是否为根节点（通过下一跳数量判断，大于1时为根节点）
    bool isRootNode = forwardValue.nextHops.size() > 1;
    
    // 如果是根节点，设置bcastArrivalState=1，即认为自己接收到了自己的广播信息
    if (isRootNode) {
      NS_LOG_INFO(m_switchId << " 检测为根节点，设置bcastArrivalState=1");
      groupState->bcastArrState[idx] = true;
      
      // 缓存聚合值到广播缓冲区
      groupState->bcastBuffer[idx] = groupState->aggBuffer[idx];
    }
    
    // 转发到所有下一跳
    for (const auto& nextHop : forwardValue.nextHops) {
      // 创建新的数据包
      Ptr<Packet> forwardPacket = Create<Packet>(groupState->packet_length);
      
      // 创建新的头部
      IncHeader forwardHeader = header;
      forwardHeader.SetSrcAddr(nextHop.srcAddr);
      forwardHeader.SetSrcQP(nextHop.srcQP);
      forwardHeader.SetDstAddr(nextHop.dstAddr);
      forwardHeader.SetDstQP(nextHop.dstQP);
      forwardHeader.SetPsn(psn); // 保持相同的PSN
      forwardHeader.SetOperation(op);
      forwardHeader.SetDataType(groupState->inc_data_type);
      forwardHeader.SetAggDataTest(groupState->aggBuffer[idx]); // 设置聚合结果
      forwardHeader.SetLength(forwardHeader.GetSerializedSize() + groupState->packet_length);
      
      // 添加头部
      forwardPacket->AddHeader(forwardHeader);
      
      // 发送数据包
      if (nextHop.socket->Send(forwardPacket) >= 0) {
        /*NS_LOG_INFO(m_switchId << " 转发聚合结果: PSN=" << psn 
                    << " 源地址=" << nextHop.srcAddr 
                    << " 目的地址=" << nextHop.dstAddr
                    << " 目的QP=" << nextHop.dstQP 
                    << " 聚合值=" << groupState->aggBuffer[idx]);*/
                    
        // 设置重传事件
        ScheduleRetransmission(forwardHeader, groupState->aggBuffer[idx]);
      } else {
        NS_LOG_ERROR(m_switchId << " 发送数据包失败");
      }
    }
  } else {
    /*NS_LOG_INFO(m_switchId << " 聚合未完成，等待更多数据");*/
  }
}

// 广播结果
void
IncSwitch::BroadcastResult(Ptr<Packet> packet, const IncHeader& header)
{
  NS_LOG_FUNCTION(this);
  
  // 获取关键信息
  Ipv4Address srcAddr = header.GetSrcAddr();
  Ipv4Address dstAddr = header.GetDstAddr();
  uint16_t dstQP = header.GetDstQP();
  uint32_t psn = header.GetPsn();
  int32_t aggDataTest = header.GetAggDataTest();
  
  // 查找转发规则
  key_with_ack forwardKey;
  forwardKey.srcAddr = srcAddr;
  forwardKey.dstAddr = dstAddr;
  forwardKey.dstQP = dstQP;
  forwardKey.isAck = false;
  
  auto forwardIt = m_forwardingTable.find(forwardKey);
  if (forwardIt == m_forwardingTable.end()) {
    NS_LOG_ERROR(m_switchId << " 未找到转发规则，无法广播结果: " 
                << srcAddr << "->" << dstAddr << ":" << dstQP);
    return;
  }
  
  const ForwardingValue& forwardValue = forwardIt->second;
  
  // 查找入站流上下文，获取组状态
  key_no_ack contextKey;
  contextKey.srcAddr = srcAddr;
  contextKey.dstAddr = dstAddr;
  contextKey.dstQP = dstQP;
  
  auto contextIt = m_inboundFlowContextTable.find(contextKey);
  if (contextIt == m_inboundFlowContextTable.end()) {
    NS_LOG_ERROR(m_switchId << " 未找到入站流上下文，无法获取组状态");
    return;
  }
  
  InboundFlowContext& context = contextIt->second;
  GroupState* groupState = context.groupStatePtr;
  
  // 转发到所有下一跳
  for (const auto& nextHop : forwardValue.nextHops) {
    // 创建新的数据包
    Ptr<Packet> broadcastPacket = Create<Packet>(groupState->packet_length);
    
    // 创建新的头部
    IncHeader broadcastHeader = header;
    broadcastHeader.SetSrcAddr(nextHop.srcAddr);
    broadcastHeader.SetSrcQP(nextHop.srcQP);
    broadcastHeader.SetDstAddr(nextHop.dstAddr);
    broadcastHeader.SetDstQP(nextHop.dstQP);
    broadcastHeader.SetPsn(psn); // 保持相同的PSN
    broadcastHeader.SetAggDataTest(aggDataTest); // 保持聚合结果
    broadcastHeader.SetLength(broadcastHeader.GetSerializedSize() + groupState->packet_length);
    
    // 添加头部
    broadcastPacket->AddHeader(broadcastHeader);
    
    // 发送数据包
    if (nextHop.socket->Send(broadcastPacket) >= 0) {
      NS_LOG_INFO(m_switchId << " 广播结果: PSN=" << psn 
                  << " 源地址=" << nextHop.srcAddr 
                  << " 目的地址=" << nextHop.dstAddr 
                  << " 目的QP=" << nextHop.dstQP 
                  << " 聚合值=" << aggDataTest);
                  
      // 设置重传事件
      ScheduleRetransmission(broadcastHeader, aggDataTest);
    } else {
      NS_LOG_ERROR(m_switchId << " 发送数据包失败");
    }
  }
}

// 处理上行ACK流
void
IncSwitch::ProcessUpstreamAck(Ptr<Packet> packet, const IncHeader& header)
{
  NS_LOG_FUNCTION(this);
  
  // 获取关键信息
  Ipv4Address srcAddr = header.GetSrcAddr();
  Ipv4Address dstAddr = header.GetDstAddr();
  uint16_t dstQP = header.GetDstQP();
  uint32_t psn = header.GetPsn();
  bool isNak = header.HasFlag(IncHeader::NACK);
  
  NS_LOG_INFO(m_switchId << " 处理" << (isNak ? "上行NAK" : "上行ACK") 
              << ": src=" << srcAddr << " dst=" << dstAddr 
              << " dstQP=" << dstQP << " PSN=" << psn);
  
  // 查找入站流上下文
  key_no_ack key;
  key.srcAddr = srcAddr;
  key.dstAddr = dstAddr;
  key.dstQP = dstQP;
  
  auto contextIt = m_inboundFlowContextTable.find(key);
  if (contextIt == m_inboundFlowContextTable.end()) {
    NS_LOG_ERROR(m_switchId << " 未找到入站流上下文，丢弃上行" 
                << (isNak ? "NAK" : "ACK") << ": " 
                << srcAddr << "->" << dstAddr << ":" << dstQP);
    return;
  }
  
  // 获取上下文和组状态
  InboundFlowContext& context = contextIt->second;
  GroupState* groupState = context.groupStatePtr;
  
  if (!groupState) {
    NS_LOG_ERROR(m_switchId << " 组状态指针为空，丢弃上行" 
                << (isNak ? "NAK" : "ACK"));
    return;
  }
  
  // 计算索引
  uint16_t idx = psn % groupState->arraySize;
  
  if (isNak) {
    // 处理NAK：如果PSN=AggPSN[idx]，将报文交给重传模块
    if (psn == groupState->aggPSN[idx]) {
      NS_LOG_INFO(m_switchId << " 收到上行NAK PSN=" << psn 
                  << "，触发重传");
      ProcessRetransmission(packet, header);
        } else {
      NS_LOG_INFO(m_switchId << " 丢弃上行NAK PSN=" << psn 
                  << " AggPSN=" << groupState->aggPSN[idx]);
    }
    return;
  }
  
  // 处理ACK
  // 取消出站流重传事件
  // 创建键，匹配outboundFlowContext的键格式，与入站方向一致
  key_no_ack outKey;
  outKey.srcAddr = srcAddr;
  outKey.dstAddr = dstAddr;
  outKey.dstQP = dstQP;
  
  auto outCtxIt = m_outboundFlowContextTable.find(outKey);
  if (outCtxIt != m_outboundFlowContextTable.end()) {
    OutboundFlowContext& outCtx = outCtxIt->second;
    
    // 查找对应PSN的重传事件
    auto eventIt = outCtx.retransmitEvents.find(psn);
    if (eventIt != outCtx.retransmitEvents.end()) {
      if (eventIt->second.IsRunning()) {
        eventIt->second.Cancel();
        NS_LOG_INFO(m_switchId << " 取消重传事件 PSN=" << psn);
      }
      outCtx.retransmitEvents.erase(eventIt);
    }
  }
  
  // 检查PSN与AggPSN的关系和广播确认报文抵达状态
  if (psn != groupState->aggPSN[idx] || context.rArrivalState[idx]) {
    NS_LOG_INFO(m_switchId << " 丢弃上行ACK: PSN=" << psn 
                << " AggPSN=" << groupState->aggPSN[idx] 
                << " RArrivalState=" << context.rArrivalState[idx]);
    return;
  }
  
  // PSN=AggPSN[idx]且RArrivalState[idx]=0的情况
  
  // 更新状态
  context.rArrivalState[idx] = true;
  context.arrivalState[idx] = false;
  groupState->rDegree[idx]++;
  
  NS_LOG_INFO(m_switchId << " 处理上行ACK: PSN=" << psn 
              << " rDegree=" << groupState->rDegree[idx] 
              << "/" << groupState->fanIn);
  
  // 检查是否收到所有子节点的确认
  if (groupState->rDegree[idx] == groupState->fanIn) {
    NS_LOG_INFO(m_switchId << " 收到所有子节点确认，清理状态 PSN=" << psn);
    
    // 清理状态
    ClearGroupState(context.groupId, idx);
    
    // 更新聚合号
    UpdateAggPSN(context.groupId, idx, groupState->arraySize);
  }
  
}

// 处理下行ACK流
void
IncSwitch::ProcessDownstreamAck(Ptr<Packet> packet, const IncHeader& header)
{
  NS_LOG_FUNCTION(this);
  
  // 获取关键信息
  Ipv4Address srcAddr = header.GetSrcAddr();
  Ipv4Address dstAddr = header.GetDstAddr();
  uint16_t dstQP = header.GetDstQP();
  uint32_t psn = header.GetPsn();
  bool isNak = header.HasFlag(IncHeader::NACK);
  
  NS_LOG_INFO(m_switchId << " 处理" << (isNak ? "下行NAK" : "下行ACK") 
              << ": src=" << srcAddr << " dst=" << dstAddr 
              << " dstQP=" << dstQP << " PSN=" << psn);
  
  // 查找入站流上下文
  key_no_ack key;
  key.srcAddr = srcAddr;
  key.dstAddr = dstAddr;
  key.dstQP = dstQP;
  
  auto contextIt = m_inboundFlowContextTable.find(key);
  if (contextIt == m_inboundFlowContextTable.end()) {
    NS_LOG_ERROR(m_switchId << " 未找到入站流上下文，丢弃下行" 
                << (isNak ? "NAK" : "ACK") << ": " 
                << srcAddr << "->" << dstAddr << ":" << dstQP);
    return;
  }
  
  // 获取上下文和组状态
  InboundFlowContext& context = contextIt->second;
  GroupState* groupState = context.groupStatePtr;
  
  if (!groupState) {
    NS_LOG_ERROR(m_switchId << " 组状态指针为空，丢弃下行" 
                << (isNak ? "NAK" : "ACK"));
    return;
  }
  
  // 计算索引
  uint16_t idx = psn % groupState->arraySize;
  
  if (isNak) {
    // 处理NAK：如果PSN=AggPSN[idx]且BcastArrivalState[idx]=0，将报文交给重传模块
    if (psn == groupState->aggPSN[idx] && !groupState->bcastArrState[idx]) {
      NS_LOG_INFO(m_switchId << " 收到下行NAK PSN=" << psn 
                  << "，触发重传");
      ProcessRetransmission(packet, header);
    } else {
      NS_LOG_INFO(m_switchId << " 丢弃下行NAK PSN=" << psn);
    }
    return;
  }
  
  // 处理ACK

  // 取消出站流重传事件
  // 创建键，匹配outboundFlowContext的键格式，与入站方向一致
  key_no_ack outKey;
  outKey.srcAddr = srcAddr;
  outKey.dstAddr = dstAddr;
  outKey.dstQP = dstQP;
  
  auto outCtxIt = m_outboundFlowContextTable.find(outKey);
  if (outCtxIt != m_outboundFlowContextTable.end()) {
    OutboundFlowContext& outCtx = outCtxIt->second;
    
    // 查找对应PSN的重传事件
    auto eventIt = outCtx.retransmitEvents.find(psn);
    if (eventIt != outCtx.retransmitEvents.end()) {
      if (eventIt->second.IsRunning()) {
        eventIt->second.Cancel();
        NS_LOG_INFO(m_switchId << " 取消重传事件 PSN=" << psn);
      }
      outCtx.retransmitEvents.erase(eventIt);
    }
  }
  
  // 检查PSN与AggPSN的关系
  if (psn != groupState->aggPSN[idx]) {
    NS_LOG_INFO(m_switchId << " 丢弃下行ACK: PSN=" << psn 
                << " AggPSN=" << groupState->aggPSN[idx]);
    return;
  }
  
}

// 发送ACK确认
void
IncSwitch::SendAck(const IncHeader& header, int32_t aggDataTest)
{
  NS_LOG_FUNCTION(this);
  
  // 获取信息
  Ipv4Address srcAddr = header.GetSrcAddr();
  Ipv4Address dstAddr = header.GetDstAddr();
  uint16_t srcQP = header.GetSrcQP();
  uint16_t dstQP = header.GetDstQP();
  uint32_t psn = header.GetPsn();
  
  // 查找入站流上下文
  key_no_ack key;
  key.srcAddr = srcAddr;
  key.dstAddr = dstAddr;
  key.dstQP = dstQP;
  
  auto contextIt = m_inboundFlowContextTable.find(key);
  if (contextIt == m_inboundFlowContextTable.end()) {
    NS_LOG_ERROR(m_switchId << " 未找到入站流上下文，无法发送ACK: " 
                << srcAddr << "->" << dstAddr << ":" << dstQP);
    return;
  }
  
  // 获取上下文
  InboundFlowContext& context = contextIt->second;
  
  // 创建ACK数据包
  Ptr<Packet> ackPacket = Create<Packet>(0);
  
  // 创建ACK头部，反转源目地址和QP
  IncHeader ackHeader;
  ackHeader.SetSrcAddr(dstAddr);  // 反转地址
  ackHeader.SetDstAddr(srcAddr);
  ackHeader.SetSrcQP(dstQP);      // 反转QP
  ackHeader.SetDstQP(srcQP);
  ackHeader.SetPsn(psn);
  ackHeader.SetOperation(header.GetOperation());
  ackHeader.SetDataType(header.GetDataType());
  ackHeader.SetFlag(IncHeader::ACK);
  ackHeader.SetGroupId(header.GetGroupId());
  ackHeader.SetAggDataTest(aggDataTest);
  ackHeader.SetLength(ackHeader.GetSerializedSize());
  
  // 添加头部
  ackPacket->AddHeader(ackHeader);
  
  // 使用Socket发送
  if (context.send_Socket->Send(ackPacket) >= 0) {
    /*NS_LOG_INFO(m_switchId << " 发送ACK: PSN=" << psn 
                << " 到=" << srcAddr << ":" << srcQP 
                << " aggDataTest=" << aggDataTest);*/
  } else {
    NS_LOG_ERROR(m_switchId << " 发送ACK失败");
  }
}

// 发送NAK确认
void
IncSwitch::SendNak(const IncHeader& header)
{
  NS_LOG_FUNCTION(this);
  
  // 获取信息
  Ipv4Address srcAddr = header.GetSrcAddr();
  Ipv4Address dstAddr = header.GetDstAddr();
  uint16_t srcQP = header.GetSrcQP();
  uint16_t dstQP = header.GetDstQP();
  uint32_t psn = header.GetPsn();
  
  // 查找入站流上下文
  key_no_ack key;
  key.srcAddr = srcAddr;
  key.dstAddr = dstAddr;
  key.dstQP = dstQP;
  
  auto contextIt = m_inboundFlowContextTable.find(key);
  if (contextIt == m_inboundFlowContextTable.end()) {
    NS_LOG_ERROR(m_switchId << " 未找到入站流上下文，无法发送NAK: " 
                << srcAddr << "->" << dstAddr << ":" << dstQP);
    return;
  }
  
  // 获取上下文和组状态
  InboundFlowContext& context = contextIt->second;
  GroupState* groupState = context.groupStatePtr;
  
  if (!groupState) {
    NS_LOG_ERROR(m_switchId << " 组状态指针为空，无法发送NAK");
    return;
  }
  
  // 计算索引
  uint16_t idx = psn % groupState->arraySize;
  
  // 获取正确的聚合号
  uint32_t aggPSN = groupState->aggPSN[idx];
  
  // 创建NAK数据包
  Ptr<Packet> nakPacket = Create<Packet>(0);
  
  // 创建NAK头部，反转源目地址和QP
  IncHeader nakHeader;
  nakHeader.SetSrcAddr(dstAddr);  // 反转地址
  nakHeader.SetDstAddr(srcAddr);
  nakHeader.SetSrcQP(dstQP);      // 反转QP
  nakHeader.SetDstQP(srcQP);
  nakHeader.SetPsn(aggPSN);       // 使用正确的聚合号，而不是原始PSN
  nakHeader.SetOperation(header.GetOperation());
  nakHeader.SetDataType(header.GetDataType());
  nakHeader.SetFlag(IncHeader::NACK);
  nakHeader.SetGroupId(header.GetGroupId());
  nakHeader.SetLength(nakHeader.GetSerializedSize());
  
  // 添加头部
  nakPacket->AddHeader(nakHeader);
  
  // 使用Socket发送
  if (context.send_Socket->Send(nakPacket) >= 0) {
    NS_LOG_INFO(m_switchId << " 发送NAK: PSN=" << aggPSN 
                << " 到=" << srcAddr << ":" << srcQP);
  } else {
    NS_LOG_ERROR(m_switchId << " 发送NAK失败");
  }
}

// 处理重传请求
void
IncSwitch::ProcessRetransmission(Ptr<Packet> packet, const IncHeader& header)
{
  NS_LOG_FUNCTION(this);
  
  // 获取关键信息
  Ipv4Address srcAddr = header.GetSrcAddr();
  Ipv4Address dstAddr = header.GetDstAddr();
  uint16_t dstQP = header.GetDstQP();
  uint32_t psn = header.GetPsn();
  
  // 查找入站流上下文
  key_no_ack key;
  key.srcAddr = srcAddr;
  key.dstAddr = dstAddr;
  key.dstQP = dstQP;
  
  auto contextIt = m_inboundFlowContextTable.find(key);
  if (contextIt == m_inboundFlowContextTable.end()) {
    NS_LOG_ERROR(m_switchId << " 未找到入站流上下文，无法处理重传: " 
                << srcAddr << "->" << dstAddr << ":" << dstQP);
    return;
  }
  
  // 获取上下文和组状态
  InboundFlowContext& context = contextIt->second;
  GroupState* groupState = context.groupStatePtr;
  
  if (!groupState) {
    NS_LOG_ERROR(m_switchId << " 组状态指针为空，无法处理重传");
    return;
  }
  
  // 计算索引
  uint16_t idx = psn % groupState->arraySize;
  
  // 获取正确的聚合号
  uint32_t aggPSN = groupState->aggPSN[idx];
  
  // 重传处理逻辑
  if (groupState->bcastArrState[idx]) {
    // 已有完整聚合结果，直接回复广播缓冲区中的值
    NS_LOG_INFO(m_switchId << " 重传聚合结果: PSN=" << psn 
                << " AggPSN=" << aggPSN
                << " 值=" << groupState->bcastBuffer[idx]);
    
    // 创建新的数据包
    Ptr<Packet> retransPacket = Create<Packet>(groupState->packet_length);
    
    // 创建新的头部，反转源目地址和QP
    IncHeader retransHeader;
    retransHeader.SetSrcAddr(dstAddr);  // 反转地址
    retransHeader.SetDstAddr(srcAddr);
    retransHeader.SetSrcQP(dstQP);      // 反转QP
    retransHeader.SetDstQP(header.GetSrcQP());
    retransHeader.SetPsn(aggPSN);       // 使用正确的聚合号，而不是原始PSN
    retransHeader.SetOperation(header.GetOperation());
    retransHeader.SetDataType(header.GetDataType());
    retransHeader.SetGroupId(header.GetGroupId());
    retransHeader.SetAggDataTest(groupState->bcastBuffer[idx]);
    retransHeader.SetLength(retransHeader.GetSerializedSize() + groupState->packet_length);
    
    // 添加头部
    retransPacket->AddHeader(retransHeader);
    
    // 使用Socket发送
    if (context.send_Socket->Send(retransPacket) >= 0) {
      NS_LOG_INFO(m_switchId << " 发送重传的聚合结果: PSN=" << aggPSN 
                  << " 到=" << srcAddr << ":" << header.GetSrcQP() 
                  << " 值=" << groupState->bcastBuffer[idx]);
                  
      // 设置重传事件
      ScheduleRetransmission(retransHeader, groupState->bcastBuffer[idx]);
    } else {
      NS_LOG_ERROR(m_switchId << " 发送重传的聚合结果失败");
    }
  } 
  else if (groupState->degree[idx] == groupState->fanIn) {
    // 已完成本节点聚合，但未广播，回复聚合缓冲区的值
    NS_LOG_INFO(m_switchId << " 重传已完成聚合的值: PSN=" << psn 
                << " AggPSN=" << aggPSN
                << " 值=" << groupState->aggBuffer[idx]);
    
    // 查找转发规则
    key_with_ack forwardKey;
    forwardKey.srcAddr = srcAddr;
    forwardKey.dstAddr = dstAddr;
    forwardKey.dstQP = dstQP;
    forwardKey.isAck = false;
    
    auto forwardIt = m_forwardingTable.find(forwardKey);
    if (forwardIt != m_forwardingTable.end()) {
      const ForwardingValue& forwardValue = forwardIt->second;
      
      // 转发到所有下一跳
      for (const auto& nextHop : forwardValue.nextHops) {
        // 创建新的数据包
        Ptr<Packet> forwardPacket = Create<Packet>(groupState->packet_length);
        
        // 创建新的头部
        IncHeader forwardHeader = header;
        forwardHeader.SetSrcAddr(nextHop.srcAddr);
        forwardHeader.SetSrcQP(nextHop.srcQP);
        forwardHeader.SetDstAddr(nextHop.dstAddr);
        forwardHeader.SetDstQP(nextHop.dstQP);
        forwardHeader.SetPsn(aggPSN);  // 使用正确的聚合号，而不是原始PSN
        forwardHeader.SetOperation(groupState->inc_op);
        forwardHeader.SetDataType(groupState->inc_data_type);
        forwardHeader.SetAggDataTest(groupState->aggBuffer[idx]);
        forwardHeader.SetLength(forwardHeader.GetSerializedSize() + groupState->packet_length);
        
        // 添加头部
        forwardPacket->AddHeader(forwardHeader);
        
        // 发送数据包
        if (nextHop.socket->Send(forwardPacket) >= 0) {
          NS_LOG_INFO(m_switchId << " 转发重传的聚合结果: PSN=" << aggPSN 
                      << " 源地址=" << nextHop.srcAddr 
                      << " 目的地址=" << nextHop.dstAddr 
                      << " 目的QP=" << nextHop.dstQP 
                      << " 值=" << groupState->aggBuffer[idx]);
                      
          // 设置重传事件
          ScheduleRetransmission(forwardHeader, groupState->aggBuffer[idx]);
        } else {
          NS_LOG_ERROR(m_switchId << " 发送重传的聚合结果失败");
        }
      }
    }
  } 
  else if (!context.arrivalState[idx]) {
    // 尚未收到子节点数据，发送NAK报文
    NS_LOG_INFO(m_switchId << " 未收到子节点数据，发送NAK: PSN=" << psn << " AggPSN=" << aggPSN);
    SendNak(header);
  } 
  else {
    // 其他情况，丢弃报文
    NS_LOG_INFO(m_switchId << " 非上述任何情况，丢弃重传请求: PSN=" << psn << " AggPSN=" << aggPSN);
  }
}

// 调度重传事件
void
IncSwitch::ScheduleRetransmission(const IncHeader& header, int32_t aggDataValue)
{
  NS_LOG_FUNCTION(this);
  
  // 从header中提取关键信息
  Ipv4Address srcAddr = header.GetSrcAddr();
  Ipv4Address dstAddr = header.GetDstAddr();
  uint16_t srcQP = header.GetSrcQP();
  uint16_t dstQP = header.GetDstQP();
  uint32_t psn = header.GetPsn();
  
  // header是重传数据包的头部，header中的src/dst是出站方向
  // key是出站流上下文键，key的src/dst是入站方向，与header相反
  key_no_ack key;
  key.srcAddr = dstAddr; // 注意：键为入站方向，所以是反的
  key.dstAddr = srcAddr;
  key.dstQP = srcQP;
  
  auto outCtxIt = m_outboundFlowContextTable.find(key);
  if (outCtxIt == m_outboundFlowContextTable.end()) {
    NS_LOG_ERROR(m_switchId << " 未找到出站流上下文，无法设置重传: " 
                << srcAddr << "->" << dstAddr << ":" << dstQP);
    return;
  }
  
  OutboundFlowContext& outCtx = outCtxIt->second;
  
  // 取消已有的重传事件（如果存在）
  auto eventIt = outCtx.retransmitEvents.find(psn);
  if (eventIt != outCtx.retransmitEvents.end() && eventIt->second.IsRunning()) {
    eventIt->second.Cancel();
  }
  
  // 保存重传值
  outCtx.retransmitValues[psn] = aggDataValue;
  
  // 调度新的重传事件
  EventId retransEvent = Simulator::Schedule(
    m_retransmitTimeout,
    &IncSwitch::RetransmitPacket,
    this,
    header, aggDataValue);
  
  // 保存事件ID
  outCtx.retransmitEvents[psn] = retransEvent;
  

}

// 执行重传
void
IncSwitch::RetransmitPacket(const IncHeader& header, int32_t aggDataValue)
{
  NS_LOG_FUNCTION(this);
  
  // 从header中提取关键信息
  Ipv4Address srcAddr = header.GetSrcAddr();
  Ipv4Address dstAddr = header.GetDstAddr();
  uint16_t srcQP = header.GetSrcQP();
  uint16_t dstQP = header.GetDstQP();
  uint32_t psn = header.GetPsn();
  uint16_t groupId = header.GetGroupId();
  
  // header是重传数据包的头部，header中的src/dst是出站方向
  // key是出站流上下文键，key的src/dst是入站方向，与header相反
  key_no_ack key;
  key.srcAddr = dstAddr; // 注意：键为入站方向，所以是反的
  key.dstAddr = srcAddr;
  key.dstQP = srcQP;
  
  auto outCtxIt = m_outboundFlowContextTable.find(key);
  if (outCtxIt == m_outboundFlowContextTable.end()) {
    NS_LOG_ERROR(m_switchId << " 未找到出站流上下文，无法重传: " 
                << srcAddr << "->" << dstAddr << ":" << dstQP);
    return;
  }
  
  OutboundFlowContext& outCtx = outCtxIt->second;
  
  // 从重传事件映射中移除该事件
  outCtx.retransmitEvents.erase(psn);
  
  // 查找组状态
  auto groupIt = m_groupStateTable.find(groupId);
  if (groupIt == m_groupStateTable.end()) {
    NS_LOG_ERROR(m_switchId << " 未找到组状态");
    return;
  }
  
  // 创建重传数据包
  Ptr<Packet> retransPacket = Create<Packet>(1024); // 默认大小1KB
  
  // 创建新的头部，拷贝原始头部的关键信息
  IncHeader retransHeader = header;
  // 下面两行似乎是多余的
  retransHeader.SetAggDataTest(aggDataValue);
  retransHeader.SetLength(retransHeader.GetSerializedSize() + 1024);
  
  // 添加头部到数据包
  retransPacket->AddHeader(retransHeader);
  
  bool packetSent = false;
  
  // 1. 首先尝试通过查询同链路反向方向的入站流上下文来获取socket
  key_no_ack inboundKey;
  inboundKey.srcAddr = dstAddr;  // 入站流的源地址是出站流的目的地址
  inboundKey.dstAddr = srcAddr;  // 入站流的目的地址是出站流的源地址
  inboundKey.dstQP = srcQP;      // 入站流的目的QP是出站流的源QP
  
  auto inboundIt = m_inboundFlowContextTable.find(inboundKey);
  if (inboundIt != m_inboundFlowContextTable.end()) {
    // 找到对应的入站流上下文，使用其socket发送数据
    InboundFlowContext& inboundCtx = inboundIt->second;
    
    if (inboundCtx.send_Socket->Send(retransPacket) >= 0) {
      NS_LOG_INFO(m_switchId << " 重传数据包: PSN=" << psn 
                << " 源地址=" << srcAddr 
                << " 目的地址=" << dstAddr 
                << " 目的QP=" << dstQP 
                << " 值=" << aggDataValue);
      packetSent = true;
    } else {
      NS_LOG_ERROR(m_switchId << " 使用入站流上下文socket发送数据包失败");
    }
  } else {
    NS_LOG_INFO(m_switchId << " 未找到入站流上下文socket，尝试其他方式");
  }
  
  
  // 2. 如果之前的方式失败，创建临时socket发送
  if (!packetSent) {
    NS_LOG_INFO(m_switchId << " 使用临时socket重传数据包");
    
    // 使用复用机制获取或创建Socket
    uint16_t srcPort = srcQP + 1024;
    Ptr<Socket> socket = GetOrCreateSocket(srcAddr, srcPort, dstAddr, 9);
    
    // 需要创建新数据包，因为前面已经添加了头部
    Ptr<Packet> newPacket = Create<Packet>(1024);
    newPacket->AddHeader(retransHeader);
    
    if (socket->Send(newPacket) >= 0) {
      NS_LOG_INFO(m_switchId << " 使用临时socket重传数据包成功: PSN=" << psn);
      packetSent = true;
    } else {
      NS_LOG_ERROR(m_switchId << " 使用临时socket重传数据包失败");
    }
  }
  
  if (packetSent) {
    NS_LOG_INFO(m_switchId << " 重传数据包成功: PSN=" << psn 
                << " 源地址=" << srcAddr 
                << " 目的地址=" << dstAddr 
                << " 目的QP=" << dstQP 
                << " 值=" << aggDataValue);
                
    // 调度下一次重传
    Time nextTimeout = m_retransmitTimeout;
    
    if (nextTimeout < Time::Max()) {
      EventId nextRetransmit = Simulator::Schedule(
        nextTimeout,
        &IncSwitch::RetransmitPacket,
        this,
        header, aggDataValue);
      
      // 保存事件ID
      outCtx.retransmitEvents[psn] = nextRetransmit;
      
      NS_LOG_INFO(m_switchId << " 设置下一次重传: PSN=" << psn 
                  << " 超时=" << nextTimeout.GetMilliSeconds() << "ms");
    }
  } else {
    NS_LOG_ERROR(m_switchId << " 所有重传方式都失败: PSN=" << psn);
  }
}

// 获取或创建Socket（Socket复用机制）
Ptr<Socket>
IncSwitch::GetOrCreateSocket(Ipv4Address srcAddr, uint16_t srcPort, Ipv4Address dstAddr, uint16_t dstPort)
{
  NS_LOG_FUNCTION(this << srcAddr << srcPort << dstAddr << dstPort);
  
  // 创建缓存键（源IP和端口）
  std::pair<Ipv4Address, uint16_t> key = std::make_pair(srcAddr, srcPort);
  
  // 查找缓存中是否已存在
  auto it = m_socketCache.find(key);
  if (it != m_socketCache.end()) {
    NS_LOG_INFO(m_switchId << " 复用已存在的Socket: " << srcAddr << ":" << srcPort);
    return it->second;
  }
  
  // 不存在则创建新的
  NS_LOG_INFO(m_switchId << " 创建新的Socket: " << srcAddr << ":" << srcPort);
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> socket = Socket::CreateSocket(GetNode(), tid);
  
  // 绑定到指定源IP地址和端口
  if (socket->Bind(InetSocketAddress(srcAddr, srcPort)) == -1) {
    NS_LOG_ERROR(m_switchId << " 无法绑定到 " << srcAddr << ":" << srcPort);
    // 即使绑定失败，也将Socket添加到缓存，避免重复尝试绑定
  }
  
  // 连接到目标地址的端口
  socket->Connect(InetSocketAddress(dstAddr, dstPort));
  
  // 将Socket添加到缓存
  m_socketCache[key] = socket;
  
  return socket;
}

} // namespace ns3 