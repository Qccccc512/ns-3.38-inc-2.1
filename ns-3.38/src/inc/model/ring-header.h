/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef RING_HEADER_H
#define RING_HEADER_H

#include "ns3/header.h"

namespace ns3 {

/**
 * \brief Ring Allreduce 协议的消息类型
 */
enum RingMessageType
{
  SCATTER_REDUCE_DATA = 1,  /**< Scatter-Reduce 阶段的数据消息 */
  ALL_GATHER_DATA = 2,      /**< All-Gather 阶段的数据消息 */
  ROUND_COMPLETE = 3,       /**< 轮次完成通知消息 */
};

/**
 * \brief Ring Allreduce 协议的头部
 * 
 * 用于在TCP载荷中传递Ring Allreduce协议的控制信息和聚合数据
 */
class RingHeader : public Header
{
public:
  /**
   * \brief 构造函数
   */
  RingHeader ();
  virtual ~RingHeader ();

  /**
   * \brief 设置消息类型
   * \param messageType 消息类型
   */
  void SetMessageType (RingMessageType messageType);
  
  /**
   * \brief 获取消息类型
   * \return 消息类型
   */
  RingMessageType GetMessageType () const;

  /**
   * \brief 设置原始数据包索引
   * \param originalPacketIndex 原始数据包索引(0 ~ m_totalPackets-1)
   */
  void SetOriginalPacketIndex (uint32_t originalPacketIndex);
  
  /**
   * \brief 获取原始数据包索引
   * \return 原始数据包索引
   */
  uint32_t GetOriginalPacketIndex () const;

  /**
   * \brief 设置用于聚合测试的数据
   * \param aggDataTest 聚合测试数据
   */
  void SetAggDataTest (int32_t aggDataTest);
  
  /**
   * \brief 获取用于聚合测试的数据
   * \return 聚合测试数据
   */
  int32_t GetAggDataTest () const;

  /**
   * \brief 设置当前传递轮次
   * \param passNumber 传递轮次(0 ~ numNodes-2)
   */
  void SetPassNumber (uint32_t passNumber);
  
  /**
   * \brief 获取当前传递轮次
   * \return 传递轮次
   */
  uint32_t GetPassNumber () const;

  /**
   * \brief 设置逻辑数据块标识
   * \param logicalChunkIdentity 逻辑数据块标识(0 ~ numNodes-1)
   */
  void SetLogicalChunkIdentity (uint32_t logicalChunkIdentity);
  
  /**
   * \brief 获取逻辑数据块标识
   * \return 逻辑数据块标识
   */
  uint32_t GetLogicalChunkIdentity () const;

  /**
   * \brief 设置发送节点ID
   * \param nodeId 发送节点ID
   */
  void SetSenderNodeId (uint32_t nodeId);
  
  /**
   * \brief 获取发送节点ID
   * \return 发送节点ID
   */
  uint32_t GetSenderNodeId () const;

  /**
   * \brief 设置当前阶段
   * \param phase 当前阶段
   */
  void SetCurrentPhase (uint32_t phase);
  
  /**
   * \brief 获取当前阶段
   * \return 当前阶段
   */
  uint32_t GetCurrentPhase () const;

  // 继承自Header的虚函数
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;

private:
  RingMessageType m_messageType;      //!< 消息类型
  uint32_t m_originalPacketIndex;     //!< 原始数据包索引
  int32_t m_aggDataTest;              //!< 用于聚合测试的数据
  uint32_t m_passNumber;              //!< 当前传递轮次
  uint32_t m_logicalChunkIdentity;    //!< 逻辑数据块标识
  uint32_t m_senderNodeId;            //!< 发送节点ID
  uint32_t m_currentPhase;            //!< 当前阶段
};

} // namespace ns3

#endif /* RING_HEADER_H */ 