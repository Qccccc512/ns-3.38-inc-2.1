/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ring-header.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RingHeader");

RingHeader::RingHeader ()
  : m_messageType (SCATTER_REDUCE_DATA),
    m_originalPacketIndex (0),
    m_aggDataTest (0),
    m_passNumber (0),
    m_logicalChunkIdentity (0),
    m_senderNodeId (0),
    m_currentPhase (0)
{
}

RingHeader::~RingHeader ()
{
}

TypeId
RingHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RingHeader")
    .SetParent<Header> ()
    .SetGroupName ("Ring")
    .AddConstructor<RingHeader> ()
  ;
  return tid;
}

TypeId
RingHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void
RingHeader::SetMessageType (RingMessageType messageType)
{
  m_messageType = messageType;
}

RingMessageType
RingHeader::GetMessageType (void) const
{
  return m_messageType;
}

void
RingHeader::SetOriginalPacketIndex (uint32_t originalPacketIndex)
{
  m_originalPacketIndex = originalPacketIndex;
}

uint32_t
RingHeader::GetOriginalPacketIndex (void) const
{
  return m_originalPacketIndex;
}

void
RingHeader::SetAggDataTest (int32_t aggDataTest)
{
  m_aggDataTest = aggDataTest;
}

int32_t
RingHeader::GetAggDataTest (void) const
{
  return m_aggDataTest;
}

void
RingHeader::SetPassNumber (uint32_t passNumber)
{
  m_passNumber = passNumber;
}

uint32_t
RingHeader::GetPassNumber (void) const
{
  return m_passNumber;
}

void
RingHeader::SetLogicalChunkIdentity (uint32_t logicalChunkIdentity)
{
  m_logicalChunkIdentity = logicalChunkIdentity;
}

uint32_t
RingHeader::GetLogicalChunkIdentity (void) const
{
  return m_logicalChunkIdentity;
}

void
RingHeader::SetSenderNodeId (uint32_t nodeId)
{
  m_senderNodeId = nodeId;
}

uint32_t
RingHeader::GetSenderNodeId () const
{
  return m_senderNodeId;
}

void
RingHeader::SetCurrentPhase (uint32_t phase)
{
  m_currentPhase = phase;
}

uint32_t
RingHeader::GetCurrentPhase () const
{
  return m_currentPhase;
}

uint32_t
RingHeader::GetSerializedSize (void) const
{
  // 计算序列化大小: 消息类型(1字节) + 原始数据包索引(4字节) + 聚合测试数据(4字节) + 传递轮次(4字节) + 逻辑数据块标识(4字节) + 发送节点ID(4字节) + 当前阶段(4字节)
  return sizeof (uint8_t)   // 消息类型
         + sizeof (uint32_t)  // 原始数据包索引
         + sizeof (int32_t)   // 聚合测试数据
         + sizeof (uint32_t)  // 传递轮次
         + sizeof (uint32_t)  // 逻辑数据块标识
         + sizeof (uint32_t)  // 发送节点ID
         + sizeof (uint32_t); // 当前阶段
}

void
RingHeader::Serialize (Buffer::Iterator start) const
{
  // 写入消息类型
  start.WriteU8 (static_cast<uint8_t> (m_messageType));
  // 写入原始数据包索引
  start.WriteHtonU32 (m_originalPacketIndex);
  // 写入聚合测试数据
  start.WriteHtonU32 (m_aggDataTest);
  // 写入传递轮次
  start.WriteHtonU32 (m_passNumber);
  // 写入逻辑数据块标识
  start.WriteHtonU32 (m_logicalChunkIdentity);
  // 写入发送节点ID
  start.WriteHtonU32 (m_senderNodeId);
  // 写入当前阶段
  start.WriteHtonU32 (m_currentPhase);
}

uint32_t
RingHeader::Deserialize (Buffer::Iterator start)
{
  // 读取消息类型
  m_messageType = static_cast<RingMessageType> (start.ReadU8 ());
  // 读取原始数据包索引
  m_originalPacketIndex = start.ReadNtohU32 ();
  // 读取聚合测试数据
  m_aggDataTest = start.ReadNtohU32 ();
  // 读取传递轮次
  m_passNumber = start.ReadNtohU32 ();
  // 读取逻辑数据块标识
  m_logicalChunkIdentity = start.ReadNtohU32 ();
  // 读取发送节点ID
  m_senderNodeId = start.ReadNtohU32 ();
  // 读取当前阶段
  m_currentPhase = start.ReadNtohU32 ();
  
  // 返回已读取的字节数
  return GetSerializedSize ();
}

void
RingHeader::Print (std::ostream &os) const
{
  os << "RingHeader: ";
  os << "消息类型=" << static_cast<uint32_t> (m_messageType);
  os << ", 原始包索引=" << m_originalPacketIndex;
  os << ", 聚合数据测试值=" << m_aggDataTest;
  os << ", 传递轮次=" << m_passNumber;
  os << ", 逻辑数据块标识=" << m_logicalChunkIdentity;
  os << ", 发送节点ID=" << m_senderNodeId;
  os << ", 当前阶段=" << m_currentPhase;
}

} // namespace ns3 