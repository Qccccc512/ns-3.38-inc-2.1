/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef RING_APPLICATION_H
#define RING_APPLICATION_H

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/address.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/tcp-socket-factory.h"
#include "ring-header.h"

#include <vector>
#include <map>

namespace ns3 {

/**
 * \brief Ring Allreduce协议的应用层状态
 */
enum RingPhase
{
  IDLE = 0,             /**< 初始状态 */
  CONNECTING = 1,       /**< 正在建立连接 */
  SCATTER_REDUCE = 2,   /**< Scatter-Reduce阶段 */
  ALL_GATHER = 3,       /**< All-Gather阶段 */
  DONE = 4              /**< 完成状态 */
};

/**
 * \brief 节点状态信息
 */
struct NodeState
{
  uint32_t nodeId;          /**< 节点ID */
  uint32_t currentPass;     /**< 当前轮次 */
  RingPhase currentPhase;   /**< 当前阶段 */
  bool readyForNextPass;    /**< 是否准备好进入下一轮 */
};

/**
 * \brief Ring Allreduce协议的应用层实现
 * 
 * 该应用实现基于TCP的Ring Allreduce协议，包括Scatter-Reduce和All-Gather两个阶段
 */
class RingApplication : public Application
{
public:
  /**
   * \brief 获取类型ID
   * \return 类型ID
   */
  static TypeId GetTypeId (void);
  
  /**
   * \brief 构造函数
   */
  RingApplication ();
  
  /**
   * \brief 析构函数
   */
  virtual ~RingApplication ();

  /**
   * \brief 设置应用参数
   * \param nodeId 节点ID
   * \param numNodes 总节点数
   * \param totalPackets 每个节点要发送的总数据包数
   * \param packetPayloadSize 数据包净荷大小(默认1024字节)
   * \param rcwndSize TCP接收窗口大小
   * \param checkInterval 状态检查间隔(毫秒)
   * \param retryInterval 重试发送间隔(毫秒)
   * \param connectionStartTime 连接建立开始时间(秒)
   * \param transferStartTime 数据传输开始时间(秒)
   * \param packetInterval 发包间隔(毫秒)
   */
  void Setup (uint32_t nodeId, uint32_t numNodes, uint32_t totalPackets, 
              uint32_t packetPayloadSize = 1024, uint32_t rcwndSize = 32 * 1024,
              uint32_t checkInterval = 10, uint32_t retryInterval = 1,
              double connectionStartTime = 0.0, double transferStartTime = 5.0,
              double packetInterval = 0.01);

  /**
   * \brief 设置对等节点地址和端口
   * \param peerAddress 下一个节点地址
   * \param peerPort 下一个节点端口
   */
  void SetPeer (Address peerAddress, uint16_t peerPort);

  /**
   * \brief 设置本地监听地址和端口
   * \param listenAddress 本地监听地址
   * \param listenPort 本地监听端口
   */
  void SetListenConfig (Address listenAddress, uint16_t listenPort);

  /**
   * \brief 获取当前阶段
   * \return 当前阶段
   */
  RingPhase GetCurrentPhase () const;

  /**
   * \brief 获取应用所属节点ID
   * \return 节点ID
   */
  uint32_t GetNodeId () const;

  /**
   * \brief 获取总节点数
   * \return 总节点数
   */
  uint32_t GetNumNodes () const;

  /**
   * \brief 获取All-Gather结果缓冲区的验证结果
   * \return 如果所有值都等于节点数则返回true
   */
  bool VerifyResults () const;

  /**
   * \brief 获取当前阶段传递轮次
   * \return 当前轮次
   */
  uint32_t GetCurrentPass () const;

  /**
   * \brief 获取每个逻辑数据块的包数量
   * \return 每个逻辑数据块的包数量
   */
  uint32_t GetPacketsPerChunk () const;

  /**
   * \brief 设置连接和传输时间
   * \param connectionStartTime 连接建立开始时间(秒)
   * \param transferStartTime 数据传输开始时间(秒)
   */
  void SetTimingParams (double connectionStartTime, double transferStartTime);

protected:
  /**
   * \brief 启动应用
   * 
   * 重写自Application基类
   */
  virtual void StartApplication (void);

  /**
   * \brief 停止应用
   * 
   * 重写自Application基类
   */
  virtual void StopApplication (void);

private:
  /**
   * \brief 初始化缓冲区
   */
  void InitializeBuffers (void);

  /**
   * \brief 开始建立连接
   */
  void StartConnectionSetup (void);

  /**
   * \brief 开始数据传输
   */
  void StartDataTransfer (void);

  /**
   * \brief 连接成功回调
   * \param socket 连接的套接字
   */
  void ConnectionSucceededCallback (Ptr<Socket> socket);

  /**
   * \brief 连接失败回调
   * \param socket 连接的套接字
   */
  void ConnectionFailedCallback (Ptr<Socket> socket);

  /**
   * \brief 接受连接回调
   * \param socket 监听套接字
   * \param from 连接来源地址
   */
  void AcceptCallback (Ptr<Socket> socket, const Address& from);

  /**
   * \brief 接收数据回调
   * \param socket 接收数据的套接字
   */
  void ReceiveCallback (Ptr<Socket> socket);

  /**
   * \brief 正常关闭回调
   * \param socket 关闭的套接字
   */
  void NormalCloseCallback (Ptr<Socket> socket);

  /**
   * \brief 错误关闭回调
   * \param socket 关闭的套接字
   */
  void ErrorCloseCallback (Ptr<Socket> socket);

  /**
   * \brief 发送循环，处理所有发送逻辑
   */
  void SendLoop (void);

  /**
   * \brief 发送轮次完成通知
   * \param pass 完成的轮次
   * \param phase 当前阶段
   */
  void SendRoundCompleteNotification (uint32_t pass, RingPhase phase);

  /**
   * \brief 检查后节点状态
   * \return 如果后节点准备好接收数据则返回true
   */
  bool IsNextNodeReady (void);

  /**
   * \brief 周期性检查后节点状态
   */
  void CheckNextNodeStatus (void);

  /**
   * \brief 处理轮次完成通知
   * \param socket 接收通知的套接字
   * \param header 消息头部
   */
  void HandleRoundCompleteNotification (Ptr<Socket> socket, const RingHeader& header);

  /**
   * \brief 计算当前轮次应接收的逻辑数据块索引
   * \return 应接收的逻辑数据块索引
   */
  uint32_t CalculateLogicalChunkToReceive (void) const;

  /**
   * \brief 检查是否可以进入下一轮
   */
  void CheckAdvanceToNextRound (void);

  /**
   * \brief 单独处理接收进入下一轮的逻辑
   */
  void AdvanceReceivingToNextRound (void);

  /**
   * \brief 单独处理发送进入下一轮的逻辑
   */
  void AdvanceSendingToNextRound (void);

  /**
   * \brief 进入下一个阶段
   */
  void AdvanceToNextPhase (void);

  /**
   * \brief 进入下一轮发送
   */
  void AdvanceToNextPass (void);

  /**
   * \brief 计算当前传递轮次中要发送的逻辑数据块索引
   * \return 逻辑数据块索引
   */
  uint32_t CalculateLogicalChunkToSend (void) const;

  /**
   * \brief 解析接收到的数据
   * \param socket 接收数据的套接字
   * \param recvBuffer 接收缓冲区
   * \param recvBufferSize 接收缓冲区大小指针
   * \return 解析处理的字节数
   */
  uint32_t ProcessReceivedData (Ptr<Socket> socket, uint8_t* recvBuffer, uint32_t* recvBufferSize);

  /**
   * \brief 记录接收包计数并检查是否完成当前数据块接收
   * \param logicalChunkIdentity 逻辑数据块标识
   * \return 如果完成了当前数据块的接收则返回true
   */
  bool RecordPacketReceiptAndCheckCompletion (uint32_t logicalChunkIdentity);

  // 成员变量
  uint32_t m_nodeId;                //!< 节点ID
  uint32_t m_numNodes;              //!< 总节点数
  uint32_t m_totalPackets;          //!< 每个节点要发送的总数据包数
  uint32_t m_packetPayloadSize;     //!< 数据包净荷大小
  uint32_t m_rcwndSize;             //!< TCP接收窗口大小
  uint32_t m_checkInterval;         //!< 状态检查间隔(毫秒)
  uint32_t m_retryInterval;         //!< 重试发送间隔(毫秒)
  double m_connectionStartTime;     //!< 连接建立开始时间(秒)
  double m_transferStartTime;       //!< 数据传输开始时间(秒)
  double m_packetInterval;          //!< 发包间隔(毫秒)
  
  Address m_peerAddress;            //!< 下一个节点地址
  uint16_t m_peerPort;              //!< 下一个节点端口
  Address m_listenAddress;          //!< 本地监听地址
  uint16_t m_listenPort;            //!< 本地监听端口
  
  Ptr<Socket> m_sendSocket;         //!< 发送套接字
  Ptr<Socket> m_listenSocket;       //!< 监听套接字
  std::vector<Ptr<Socket>> m_connectionSockets;  //!< 已建立连接的套接字
  
  //!< 缓冲区，二者应该合二为一，这里在写的时候分开了
  std::vector<int32_t> m_scatterReduceBuffer;   //!< Scatter-Reduce缓冲区
  std::vector<int32_t> m_allGatherBuffer;       //!< All-Gather结果缓冲区
  
  RingPhase m_currentPhase;         //!< 当前阶段
  uint32_t m_packetsPerChunk;       //!< 每个逻辑数据块的包数量
  uint32_t m_currentPass;           //!< 当前传递轮次
  
  uint32_t m_packetsSentForCurrentLogicalChunkInPass;       //!< 当前轮次中当前逻辑数据块已发送的包数量
  std::vector<uint32_t> m_packetsReceivedForLogicalChunksInPass;  //!< 当前轮次中各逻辑数据块已接收的包数量
  
  EventId m_sendEvent;              //!< 发送事件
  EventId m_checkStatusEvent;       //!< 检查状态事件
  
  // 状态同步相关
  bool m_waitingForNextNode;        //!< 是否正在等待后节点准备就绪
  bool m_hasNotifiedPreviousNode;   //!< 是否已通知前节点本轮次完成
  bool m_isInitialRound;            //!< 是否为初始轮次
  bool m_canSend;                   //!< 是否可以发送
  bool m_receiveReady;              //!< 是否可以开始下一轮接收
  bool m_sendReady;                 //!< 是否可以开始下一轮发送
  
  // 后节点状态
  NodeState m_nextNodeState;        //!< 后节点状态
  
  // 用于TCP粘包/分包处理的变量
  std::map<Ptr<Socket>, std::vector<uint8_t>> m_socketBuffers;  //!< 套接字缓冲区
  
  // 用于统计的变量
  Time m_startTime;                 //!< 开始时间
  Time m_endTime;                   //!< 结束时间
  Time m_connectionStartRealTime;   //!< 实际连接开始时间
  
  // 跟踪回调
  TracedCallback<Ptr<const Packet>> m_txTrace;   //!< 发送跟踪
  TracedCallback<Ptr<const Packet>> m_rxTrace;   //!< 接收跟踪
};

} // namespace ns3

#endif /* RING_APPLICATION_H */ 