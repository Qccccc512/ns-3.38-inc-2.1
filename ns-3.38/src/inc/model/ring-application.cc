/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ring-application.h"
#include "ring-header.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/boolean.h"
#include "ns3/double.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RingApplication");

NS_OBJECT_ENSURE_REGISTERED (RingApplication);

TypeId
RingApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RingApplication")
    .SetParent<Application> ()
    .SetGroupName ("Ring")
    .AddConstructor<RingApplication> ()
    .AddAttribute ("NodeId", "节点ID",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RingApplication::m_nodeId),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("NumNodes", "总节点数",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RingApplication::m_numNodes),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("TotalPackets", "每个节点要发送的总数据包数",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RingApplication::m_totalPackets),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("PacketPayloadSize", "每个数据包的净荷大小",
                   UintegerValue (1024),
                   MakeUintegerAccessor (&RingApplication::m_packetPayloadSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("RcwndSize", "TCP接收窗口大小",
                   UintegerValue (32 * 1024),
                   MakeUintegerAccessor (&RingApplication::m_rcwndSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("CheckInterval", "检查间隔时间",
                   UintegerValue (10),
                   MakeUintegerAccessor (&RingApplication::m_checkInterval),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("RetryInterval", "重试发送间隔时间",
                   UintegerValue (1),
                   MakeUintegerAccessor (&RingApplication::m_retryInterval),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("PacketInterval", "发包时间间隔(毫秒)",
                   DoubleValue (0.01),
                   MakeDoubleAccessor (&RingApplication::m_packetInterval),
                   MakeDoubleChecker<double> (0.0))
    .AddTraceSource ("Tx", "发送跟踪",
                     MakeTraceSourceAccessor (&RingApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("Rx", "接收跟踪",
                     MakeTraceSourceAccessor (&RingApplication::m_rxTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}

RingApplication::RingApplication ()
  : m_nodeId (0),
    m_numNodes (0),
    m_totalPackets (0),
    m_packetPayloadSize (1024),
    m_rcwndSize (32 * 1024),
    m_checkInterval (10),
    m_retryInterval (1),
    m_connectionStartTime (0.0),
    m_transferStartTime (5.0),
    m_packetInterval (0.01),
    m_peerPort (0),
    m_listenPort (0),
    m_sendSocket (0),
    m_listenSocket (0),
    m_currentPhase (IDLE),
    m_packetsPerChunk (0),
    m_currentPass (0),
    m_packetsSentForCurrentLogicalChunkInPass (0),
    m_waitingForNextNode (false),
    m_hasNotifiedPreviousNode (false),
    m_isInitialRound (true),
    m_canSend (false),
    m_receiveReady (false),
    m_sendReady (false)
{
  NS_LOG_FUNCTION (this);
  // 初始化后节点状态
  m_nextNodeState.nodeId = 0;
  m_nextNodeState.currentPass = 0;
  m_nextNodeState.currentPhase = IDLE;
  m_nextNodeState.readyForNextPass = false;
}

RingApplication::~RingApplication ()
{
  NS_LOG_FUNCTION (this);
}

void
RingApplication::Setup (uint32_t nodeId, uint32_t numNodes, uint32_t totalPackets, 
                        uint32_t packetPayloadSize, uint32_t rcwndSize, uint32_t checkInterval,
                        uint32_t retryInterval, double connectionStartTime, double transferStartTime,
                        double packetInterval)
{
  NS_LOG_FUNCTION (this << nodeId << numNodes << totalPackets << packetPayloadSize 
                  << rcwndSize << checkInterval << retryInterval 
                  << connectionStartTime << transferStartTime << packetInterval);
  
  m_nodeId = nodeId;
  m_numNodes = numNodes;
  m_totalPackets = totalPackets;
  m_checkInterval = checkInterval;
  m_retryInterval = retryInterval;
  m_connectionStartTime = connectionStartTime;
  m_transferStartTime = transferStartTime;
  m_packetInterval = packetInterval;
  
  // 计算RingHeader大小
  // RingHeader header;
  // uint32_t headerSize = header.GetSerializedSize();

  m_packetPayloadSize = packetPayloadSize;
  
  
  m_rcwndSize = rcwndSize;
  
  // 计算每个逻辑数据块的包数量
  if (m_totalPackets % m_numNodes != 0)
    {
      NS_FATAL_ERROR ("totalPackets必须能被numNodes整除");
    }
  m_packetsPerChunk = m_totalPackets / m_numNodes;
  
  // 初始化接收计数数组
  m_packetsReceivedForLogicalChunksInPass.resize (m_numNodes, 0);
  
  // 初始化缓冲区
  InitializeBuffers ();
}

void
RingApplication::SetPeer (Address peerAddress, uint16_t peerPort)
{
  NS_LOG_FUNCTION (this << peerAddress << peerPort);
  m_peerAddress = peerAddress;
  m_peerPort = peerPort;
}

void
RingApplication::SetListenConfig (Address listenAddress, uint16_t listenPort)
{
  NS_LOG_FUNCTION (this << listenAddress << listenPort);
  m_listenAddress = listenAddress;
  m_listenPort = listenPort;
}

RingPhase
RingApplication::GetCurrentPhase () const
{
  return m_currentPhase;
}

uint32_t
RingApplication::GetNodeId () const
{
  return m_nodeId;
}

uint32_t
RingApplication::GetNumNodes () const
{
  return m_numNodes;
}

bool
RingApplication::VerifyResults () const
{
  for (uint32_t i = 0; i < m_totalPackets; ++i)
    {
      if (m_allGatherBuffer[i] != static_cast<int32_t>(m_numNodes))
        {
          return false;
        }
    }
  return true;
}

uint32_t
RingApplication::GetCurrentPass () const
{
  return m_currentPass;
}

uint32_t
RingApplication::GetPacketsPerChunk () const
{
  return m_packetsPerChunk;
}

void
RingApplication::StartApplication (void)
{
  NS_LOG_FUNCTION (this);
  
  // 延迟建立连接
  if (m_connectionStartTime > 0.0)
    {
      Time delay = Seconds(m_connectionStartTime);
      NS_LOG_INFO ("节点 " << m_nodeId << " 将在 " << m_connectionStartTime << " 秒后开始建立连接");
      Simulator::Schedule (delay, &RingApplication::StartConnectionSetup, this);
    }
  else
    {
      // 立即开始建立连接
      StartConnectionSetup ();
    }
}

void
RingApplication::StartConnectionSetup (void)
{
  NS_LOG_FUNCTION (this);
  
  // 记录实际连接开始时间
  m_connectionStartRealTime = Simulator::Now ();
  NS_LOG_INFO ("节点 " << m_nodeId << " 开始建立连接，实际时间: " << m_connectionStartRealTime.GetSeconds () << "秒");
  
  // 创建发送套接字
  if (!m_sendSocket)
    {
      TypeId tid = TcpSocketFactory::GetTypeId ();
      m_sendSocket = Socket::CreateSocket (GetNode (), tid);
      
      // 设置TCP缓冲区大小
      m_sendSocket->SetAttribute ("SndBufSize", UintegerValue (m_rcwndSize));
      m_sendSocket->SetAttribute ("RcvBufSize", UintegerValue (m_rcwndSize));
      
      // 添加回调
      m_sendSocket->SetConnectCallback (
        MakeCallback (&RingApplication::ConnectionSucceededCallback, this),
        MakeCallback (&RingApplication::ConnectionFailedCallback, this));
      m_sendSocket->SetCloseCallbacks (
        MakeCallback (&RingApplication::NormalCloseCallback, this),
        MakeCallback (&RingApplication::ErrorCloseCallback, this));
      m_sendSocket->SetRecvCallback (
        MakeCallback (&RingApplication::ReceiveCallback, this));
      
      // 连接到对等节点
      if (Ipv4Address::IsMatchingType (m_peerAddress))
        {
          m_sendSocket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (m_peerAddress), m_peerPort));
        }
      else if (Ipv6Address::IsMatchingType (m_peerAddress))
        {
          m_sendSocket->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom (m_peerAddress), m_peerPort));
        }
      
      m_currentPhase = CONNECTING;
    }
  
  // 创建监听套接字
  if (!m_listenSocket)
    {
      TypeId tid = TcpSocketFactory::GetTypeId ();
      m_listenSocket = Socket::CreateSocket (GetNode (), tid);
      
      // 设置TCP缓冲区大小
      m_listenSocket->SetAttribute ("RcvBufSize", UintegerValue (m_rcwndSize));
      
      // 绑定监听地址和端口
      if (Ipv4Address::IsMatchingType (m_listenAddress))
        {
          m_listenSocket->Bind (InetSocketAddress (Ipv4Address::ConvertFrom (m_listenAddress), m_listenPort));
        }
      else if (Ipv6Address::IsMatchingType (m_listenAddress))
        {
          m_listenSocket->Bind (Inet6SocketAddress (Ipv6Address::ConvertFrom (m_listenAddress), m_listenPort));
        }
      
      // 开始监听
      m_listenSocket->Listen ();
      
      // 添加回调
      m_listenSocket->SetAcceptCallback (
        MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
        MakeCallback (&RingApplication::AcceptCallback, this));
    }
}

void
RingApplication::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
  
  if (m_currentPhase != DONE)
    {
      m_endTime = Simulator::Now ();
      m_currentPhase = DONE;
      
      // 输出结果验证信息，使用实际连接开始时间计算总耗时
      double totalTime = (m_endTime - m_connectionStartRealTime).GetSeconds ();
      NS_LOG_ERROR ("节点 " << m_nodeId << " Ring Allreduce疑似未完成，耗时 " 
                    << totalTime << " 秒 (传输耗时: " 
                    << (m_endTime - m_startTime).GetSeconds () << " 秒)");
      NS_LOG_ERROR ("验证结果: " << (VerifyResults () ? "成功" : "失败"));
    }
  
  // 关闭所有套接字
  if (m_sendSocket)
    {
      m_sendSocket->Close ();
      m_sendSocket = 0;
    }
  
  if (m_listenSocket)
    {
      m_listenSocket->Close ();
      m_listenSocket = 0;
    }
  
  for (auto socket : m_connectionSockets)
    {
      socket->Close ();
    }
  m_connectionSockets.clear ();
  
  // 取消挂起的事件
  if (m_sendEvent.IsRunning ())
    {
      m_sendEvent.Cancel ();
    }
}

void
RingApplication::InitializeBuffers (void)
{
  NS_LOG_FUNCTION (this);
  
  // 初始化Scatter-Reduce缓冲区，所有值都设为1
  m_scatterReduceBuffer.resize (m_totalPackets, 1);
  
  // 初始化All-Gather缓冲区，所有值都设为0
  m_allGatherBuffer.resize (m_totalPackets, 0);
}

void
RingApplication::ConnectionSucceededCallback (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_INFO ("节点 " << m_nodeId << " 成功连接到对等节点");
  
  // 连接建立，但不立即开始Ring Allreduce操作
  if (m_currentPhase == CONNECTING)
    {
      if (m_transferStartTime > 0.0)
        {
          // 在指定时间点开始传输数据
          Time now = Simulator::Now ();
          double delaySeconds = m_transferStartTime - now.GetSeconds ();
          
          if (delaySeconds > 0.0)
            {
              NS_LOG_INFO ("节点 " << m_nodeId << " 将在 " << m_transferStartTime 
                          << " 秒时开始数据传输，当前时间 " << now.GetSeconds () << " 秒");
              Time delay = Seconds(delaySeconds);
              Simulator::Schedule (delay, &RingApplication::StartDataTransfer, this);
            }
          else
            {
              // 如果当前时间已经超过了传输开始时间，立即开始
              StartDataTransfer ();
            }
        }
      else
        {
          // 如果没有指定传输开始时间，立即开始
          StartDataTransfer ();
        }
    }
}

void
RingApplication::StartDataTransfer (void)
{
  NS_LOG_FUNCTION (this);
  
  // 记录开始时间
  m_startTime = Simulator::Now ();
  NS_LOG_INFO ("节点 " << m_nodeId << " 开始数据传输，时间: " << m_startTime.GetSeconds () << "秒");
  
  // 进入Scatter-Reduce阶段，开始Ring Allreduce
  m_currentPhase = SCATTER_REDUCE;
  m_currentPass = 0;
  m_isInitialRound = true;  // 设置为初始轮次
  m_canSend = true;         // 初始轮次允许发送
  m_receiveReady = false;    // 初始轮次允许接收
  m_sendReady = false;       // 初始轮次允许发送
  
  // 重置发送和接收计数
  m_packetsSentForCurrentLogicalChunkInPass = 0;
  std::fill (m_packetsReceivedForLogicalChunksInPass.begin (), 
             m_packetsReceivedForLogicalChunksInPass.end (), 0);
  
  // 开始发送
  SendLoop ();
}

void
RingApplication::ConnectionFailedCallback (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_ERROR ("节点 " << m_nodeId << " 连接对等节点失败");
}

void
RingApplication::AcceptCallback (Ptr<Socket> socket, const Address& from)
{
  NS_LOG_FUNCTION (this << socket << from);
  NS_LOG_INFO ("节点 " << m_nodeId << " 接受来自对等节点的连接");
  
  // 接受新连接
  // 传入的socket就是已经接受的新连接，不需要调用Accept()或GetAcceptedSocket()
  Ptr<Socket> connectionSocket = socket;
  
  // 设置TCP缓冲区大小
  connectionSocket->SetAttribute ("RcvBufSize", UintegerValue (m_rcwndSize));
  
  // 添加回调
  connectionSocket->SetRecvCallback (
    MakeCallback (&RingApplication::ReceiveCallback, this));
  connectionSocket->SetCloseCallbacks (
    MakeCallback (&RingApplication::NormalCloseCallback, this),
    MakeCallback (&RingApplication::ErrorCloseCallback, this));
  
  // 保存连接套接字
  m_connectionSockets.push_back (connectionSocket);
}

void
RingApplication::ReceiveCallback (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  
  // 获取或创建该socket的缓冲区
  if (m_socketBuffers.find (socket) == m_socketBuffers.end ())
    {
      m_socketBuffers[socket] = std::vector<uint8_t> ();
    }
  std::vector<uint8_t>& buffer = m_socketBuffers[socket];
  
  // 接收数据
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      // 跟踪接收
      m_rxTrace (packet);
      
      // 将数据追加到缓冲区
      uint32_t size = packet->GetSize ();
      uint32_t oldSize = buffer.size ();
      buffer.resize (oldSize + size);
      packet->CopyData (&buffer[oldSize], size);
      
      NS_LOG_DEBUG ("节点 " << m_nodeId << " 接收了 " << size << " 字节，缓冲区大小现在为 " << buffer.size ());
    }
  
  // 处理接收到的数据
  uint32_t bufferSize = buffer.size();
  uint32_t processedBytes = ProcessReceivedData (socket, buffer.data(), &bufferSize);
  
  // 移除已处理的数据
  if (processedBytes > 0)
    {
      if (bufferSize > 0)
        {
          // 保留未处理的数据
          std::vector<uint8_t> newBuffer(buffer.begin() + processedBytes, buffer.end());
          buffer.swap(newBuffer);
        }
      else
        {
          // 所有数据都已处理，清空缓冲区
          buffer.clear();
        }
    }
}

uint32_t
RingApplication::ProcessReceivedData (Ptr<Socket> socket, uint8_t* recvBuffer, uint32_t* recvBufferSize)
{
  NS_LOG_FUNCTION (this << socket << recvBuffer << *recvBufferSize);
  
  uint32_t processedBytes = 0;
  
  while (processedBytes < *recvBufferSize)
    {
      // 检查剩余数据是否足够构成一个完整的RingHeader
      uint32_t remainingBytes = *recvBufferSize - processedBytes;
      RingHeader header;
      uint32_t headerSize = header.GetSerializedSize ();
      
      if (remainingBytes < headerSize)
        {
          // 数据不足，等待更多数据
          break;
        }
      
      // 创建临时缓冲区用于反序列化头部
      Buffer tempBuffer;
      tempBuffer.AddAtStart (remainingBytes);
      Buffer::Iterator tempStart = tempBuffer.Begin ();
      
      // 复制数据到临时缓冲区
      for (uint32_t i = 0; i < remainingBytes; i++)
        {
          tempStart.WriteU8(recvBuffer[processedBytes + i]);
        }
      
      // 回到缓冲区开始位置进行反序列化
      tempStart = tempBuffer.Begin();
      header.Deserialize (tempStart);
      
      // 计算完整包大小 (头部 + 负载)
      uint32_t fullPacketSize = headerSize + m_packetPayloadSize;
      
      if (remainingBytes < fullPacketSize)
        {
          // 数据不足以构成完整包，等待更多数据
          break;
        }
      
      // 成功获取一个完整包，处理它
      NS_LOG_DEBUG ("节点 " << m_nodeId 
                    << " 正在处理完整数据包: 消息类型=" << static_cast<uint32_t>(header.GetMessageType ())
                    << ", 原始包索引=" << header.GetOriginalPacketIndex ()
                    << ", 聚合数据=" << header.GetAggDataTest ()
                    << ", 轮次=" << header.GetPassNumber ()
                    << ", 数据块ID=" << header.GetLogicalChunkIdentity ());
      
      // 根据消息类型处理数据
      if (header.GetMessageType() == ROUND_COMPLETE)
        {
          // 处理轮次完成通知
          HandleRoundCompleteNotification(socket, header);
        }
      else if (m_currentPhase == SCATTER_REDUCE && header.GetMessageType () == SCATTER_REDUCE_DATA)
        {
          // 验证聚合数据
          uint32_t expectedAggData = header.GetPassNumber () + 1;
          if (header.GetAggDataTest () != static_cast<int32_t>(expectedAggData))
            {
              NS_LOG_WARN ("节点 " << m_nodeId << " 接收到无效的聚合数据: "
                          << header.GetAggDataTest () << ", 期望值: " << expectedAggData);
            }
          
          // 更新Scatter-Reduce缓冲区
          uint32_t opi = header.GetOriginalPacketIndex ();
          m_scatterReduceBuffer[opi] += header.GetAggDataTest ();
          
          // 记录数据块接收进度
          uint32_t logicalChunkId = header.GetLogicalChunkIdentity ();
          bool chunkCompleted = RecordPacketReceiptAndCheckCompletion (logicalChunkId);
          
          // 如果完成了逻辑数据块接收，检查是否可以进入下一轮
          if (chunkCompleted)
            {
              // 检查是否是当前轮次应接收的数据块
              uint32_t expectedChunk = CalculateLogicalChunkToReceive();
              if (logicalChunkId == expectedChunk)
                {
                  // 当前轮次应接收的数据块已完成，检查是否可以进入下一轮
                  CheckAdvanceToNextRound();
                }
            }
        }
      else if (m_currentPhase == ALL_GATHER && header.GetMessageType () == ALL_GATHER_DATA)
        {
          // 验证聚合数据
          if (header.GetAggDataTest () != static_cast<int32_t>(m_numNodes))
            {
              NS_LOG_WARN ("节点 " << m_nodeId << " 在All-Gather阶段接收到无效的聚合数据: "
                          << header.GetAggDataTest () << ", 期望值: " << m_numNodes);
            }
          
          // 更新两个缓冲区
          uint32_t opi = header.GetOriginalPacketIndex ();
          m_scatterReduceBuffer[opi] = header.GetAggDataTest ();  // 更新工作缓冲区
          m_allGatherBuffer[opi] = header.GetAggDataTest ();      // 更新最终结果缓冲区
          
          // 记录数据块接收进度
          uint32_t logicalChunkId = header.GetLogicalChunkIdentity ();
          bool chunkCompleted = RecordPacketReceiptAndCheckCompletion (logicalChunkId);
          
          // 如果完成了逻辑数据块接收，检查是否可以进入下一轮
          if (chunkCompleted)
            {
              // 检查是否是当前轮次应接收的数据块
              uint32_t expectedChunk = CalculateLogicalChunkToReceive();
              if (logicalChunkId == expectedChunk)
                {
                  // 当前轮次应接收的数据块已完成，检查是否可以进入下一轮
                  CheckAdvanceToNextRound();
                }
            }
        }
      else
        {
          NS_LOG_WARN ("节点 " << m_nodeId << " 接收到意外的消息类型 "
                      << static_cast<uint32_t>(header.GetMessageType ())
                      << " 在阶段 " << static_cast<uint32_t>(m_currentPhase));
        }
      
      // 更新处理进度
      processedBytes += fullPacketSize;
    }
  
  // 更新接收缓冲区大小
  *recvBufferSize -= processedBytes;
  
  return processedBytes;
}

bool
RingApplication::RecordPacketReceiptAndCheckCompletion (uint32_t logicalChunkIdentity)
{
  NS_LOG_FUNCTION (this << logicalChunkIdentity);
  
  // 增加该逻辑数据块的接收计数
  m_packetsReceivedForLogicalChunksInPass[logicalChunkIdentity]++;
  
  // 检查是否完成了该逻辑数据块的接收
  if (m_packetsReceivedForLogicalChunksInPass[logicalChunkIdentity] >= m_packetsPerChunk)
    {
      NS_LOG_INFO ("节点 " << m_nodeId << " 在轮次 " << m_currentPass 
                  << " 中完成接收逻辑数据块 " << logicalChunkIdentity);
      return true;
    }
  
  return false;
}

void
RingApplication::NormalCloseCallback (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_INFO ("节点 " << m_nodeId << " 套接字正常关闭");
}

void
RingApplication::ErrorCloseCallback (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_ERROR ("节点 " << m_nodeId << " 套接字出错关闭");
}

void
RingApplication::HandleRoundCompleteNotification (Ptr<Socket> socket, const RingHeader& header)
{
  NS_LOG_FUNCTION (this << socket);
  
  uint32_t senderNodeId = header.GetSenderNodeId();
  uint32_t senderPass = header.GetPassNumber();
  uint32_t senderPhase = header.GetCurrentPhase();
  
  NS_LOG_INFO ("节点 " << m_nodeId << " 收到来自节点 " << senderNodeId 
               << " 的轮次完成通知: 轮次=" << senderPass 
               << ", 阶段=" << senderPhase);
  
  // 更新后节点状态
  if (senderNodeId == (m_nodeId + 1) % m_numNodes)
    {
      m_nextNodeState.nodeId = senderNodeId;
      m_nextNodeState.currentPass = senderPass;
      m_nextNodeState.currentPhase = static_cast<RingPhase>(senderPhase);
      m_nextNodeState.readyForNextPass = true;
      
      // 设置可以发送
      m_sendReady = true;
      
      NS_LOG_INFO ("节点 " << m_nodeId << " 更新后节点 " << senderNodeId 
                   << " 状态: 准备就绪，允许发送");
      
      // 如果正在等待后节点准备就绪，可以继续发送
      if (m_waitingForNextNode)
        {
          m_waitingForNextNode = false;
          m_canSend = true;
          m_nextNodeState.readyForNextPass = false;
          
          // 如果当前未发送任何数据包，则开始发送
          if (m_packetsSentForCurrentLogicalChunkInPass == 0)
            {
              SendLoop();
            }
        }
    }
}

void
RingApplication::SendRoundCompleteNotification (uint32_t pass, RingPhase phase)
{
  NS_LOG_FUNCTION (this << pass << static_cast<uint32_t>(phase));
  
  // 找到前节点的连接套接字
  // 对于节点i，前节点是(i-1+m_numNodes)%m_numNodes
  uint32_t prevNodeId = (m_nodeId - 1 + m_numNodes) % m_numNodes;
  
  // 创建轮次完成通知
  RingHeader header;
  header.SetMessageType (ROUND_COMPLETE);
  header.SetPassNumber (pass);
  header.SetCurrentPhase (static_cast<uint32_t>(phase));
  header.SetSenderNodeId (m_nodeId);
  
  // 创建一个空的数据包并添加头部
  Ptr<Packet> packet = Create<Packet> (m_packetPayloadSize);
  packet->AddHeader (header);
  
  NS_LOG_INFO ("节点 " << m_nodeId << " 发送轮次完成通知给节点 " << prevNodeId
               << ": 轮次=" << pass << ", 阶段=" << static_cast<uint32_t>(phase));
  
  // 发送给前一个节点
  bool notificationSent = false;
  for (auto socket : m_connectionSockets)
    {
      // 假设第一个建立的连接是来自前一个节点的
      // 可能需要更精确的方法
      int actualBytes = socket->Send (packet);
      if (actualBytes > 0)
        {
          // 跟踪发送
          m_txTrace (packet);
          notificationSent = true;
          NS_LOG_INFO ("节点 " << m_nodeId << " 成功发送轮次完成通知");
          break;
        }
    }
  
  if (!notificationSent)
    {
      NS_LOG_WARN ("节点 " << m_nodeId << " 未能发送轮次完成通知，将重试");
      // 稍后重试
      Simulator::Schedule (MilliSeconds(m_retryInterval), &RingApplication::SendRoundCompleteNotification, 
                          this, pass, phase);
    }
}

bool
RingApplication::IsNextNodeReady (void)
{
  NS_LOG_FUNCTION (this);
  return m_nextNodeState.readyForNextPass;
}

void
RingApplication::CheckNextNodeStatus (void)
{
  NS_LOG_FUNCTION (this);
  
  if (m_waitingForNextNode && !m_nextNodeState.readyForNextPass)
    {
      NS_LOG_INFO ("节点 " << m_nodeId << " 仍在等待后节点准备就绪");
      
      // 再次检查
      m_checkStatusEvent = Simulator::Schedule (MilliSeconds(m_checkInterval), 
                                               &RingApplication::CheckNextNodeStatus, 
                                               this);
    }
  else if (m_waitingForNextNode && m_nextNodeState.readyForNextPass)
    {
      // 后节点已准备好，可以发送
      m_waitingForNextNode = false;
      m_canSend = true;
      
      // 开始发送
      SendLoop();
    }
}

void
RingApplication::AdvanceToNextPass (void)
{
  NS_LOG_FUNCTION (this);
  
  // 增加当前轮次
  m_currentPass++;
  m_isInitialRound = false;  // 不再是初始轮次
  
  // 重置发送和接收计数
  m_packetsSentForCurrentLogicalChunkInPass = 0;
  std::fill (m_packetsReceivedForLogicalChunksInPass.begin (), 
             m_packetsReceivedForLogicalChunksInPass.end (), 0);
  
  // 重置状态同步标志
  m_hasNotifiedPreviousNode = false;
  //m_nextNodeState.readyForNextPass = false;
  //m_waitingForNextNode = false;
  
  // 新的状态管理
  m_receiveReady = true;  // 始终可以进入下一轮接收  //无用
  m_sendReady = m_nextNodeState.readyForNextPass;  // 只有初始轮次可以直接发送，否则需等待后节点
  m_canSend = m_nextNodeState.readyForNextPass;
  
  NS_LOG_INFO ("节点 " << m_nodeId << " 在阶段 " << static_cast<uint32_t>(m_currentPhase)
              << " 中进入轮次 " << m_currentPass);
  
  // 开始新一轮的发送（如果可以发送）
  /*if (m_canSend)
    {
      SendLoop ();
    }*/
}

void
RingApplication::AdvanceToNextPhase (void)
{
  NS_LOG_FUNCTION (this);
  
  if (m_currentPhase == SCATTER_REDUCE)
    {
      m_currentPhase = ALL_GATHER;
      m_currentPass = 0;
      m_isInitialRound = true;  // 新阶段的第一轮视为初始轮次
      
      // 重置发送和接收计数
      m_packetsSentForCurrentLogicalChunkInPass = 0;
      std::fill (m_packetsReceivedForLogicalChunksInPass.begin (), 
                 m_packetsReceivedForLogicalChunksInPass.end (), 0);
      
      // 重置状态同步标志
      m_hasNotifiedPreviousNode = false;
      //m_nextNodeState.readyForNextPass = false;
      //m_waitingForNextNode = false;
      
      // 新的状态管理
      m_receiveReady = true;  // 始终可以进入下一轮接收  //无用
      m_sendReady = m_nextNodeState.readyForNextPass;  
      m_canSend = m_nextNodeState.readyForNextPass;       
      
      // 在进入AllGather阶段时，将完成Scatter-Reduce的数据放入结果缓冲区
      // 识别出当前节点负责的最终聚合数据
      uint32_t myChunk = (m_nodeId + 1) % m_numNodes;
      for (uint32_t i = 0; i < m_packetsPerChunk; i++)
        {
          uint32_t opi = myChunk * m_packetsPerChunk + i;
          if (opi < m_totalPackets && m_scatterReduceBuffer[opi] == static_cast<int32_t>(m_numNodes))
            {
              m_allGatherBuffer[opi] = m_scatterReduceBuffer[opi];
            }
        }
      
      NS_LOG_INFO ("节点 " << m_nodeId << " 进入ALL_GATHER阶段");
      
      // 开始All-Gather阶段的发送
      //SendLoop ();
    }
}

void
RingApplication::SendLoop (void)
{
  NS_LOG_FUNCTION (this);
  
  // 检查是否有挂起的发送事件
  if (m_sendEvent.IsRunning ())
    {
      // 有挂起的发送事件，取消它
      m_sendEvent.Cancel ();
    }
  
  // 检查当前阶段和发送状态
  if (m_currentPhase != SCATTER_REDUCE && m_currentPhase != ALL_GATHER)
    {
      return;
    }
  
  // 检查是否已完成当前逻辑数据块的发送
  if (m_packetsSentForCurrentLogicalChunkInPass >= m_packetsPerChunk)
    {
      // 已完成当前逻辑数据块的发送，检查是否可以进入下一轮
      m_canSend = false;
      CheckAdvanceToNextRound();
      return;
    }
  
  // 检查是否可以发送（必须等待后节点准备就绪）
  if (!m_canSend)
    {
      NS_LOG_INFO ("节点 " << m_nodeId << " 等待后节点准备就绪后再发送数据");
      m_waitingForNextNode = true;
      // 不需要检查，如果后节点未准备好，则不会发送数据
      // 后节点会主动通知前节点，前节点会等待后节点准备好后再发送数据
      /*if (!m_checkStatusEvent.IsRunning())
        {
          m_checkStatusEvent = Simulator::Schedule (MilliSeconds(m_checkInterval), 
                                                  &RingApplication::CheckNextNodeStatus, 
                                                  this);
        }*/
      return;
    }
  
  // 计算当前要发送的逻辑数据块索引
  uint32_t logicalChunkToSend = CalculateLogicalChunkToSend ();
  
  // 计算该逻辑数据块中当前待发送包的原始数据包索引
  uint32_t opi = logicalChunkToSend * m_packetsPerChunk + m_packetsSentForCurrentLogicalChunkInPass;
  
  // 创建RingHeader
  RingHeader header;
  
  if (m_currentPhase == SCATTER_REDUCE)
    {
      header.SetMessageType (SCATTER_REDUCE_DATA);
      header.SetOriginalPacketIndex (opi);
      header.SetAggDataTest (m_scatterReduceBuffer[opi]);  // 此时值应为 k+1
      header.SetPassNumber (m_currentPass);
      header.SetLogicalChunkIdentity (logicalChunkToSend);
      header.SetSenderNodeId (m_nodeId);
      header.SetCurrentPhase (static_cast<uint32_t>(m_currentPhase));
      
      NS_LOG_DEBUG ("节点 " << m_nodeId << " 发送SCATTER_REDUCE_DATA: opi=" << opi 
                   << ", aggData=" << m_scatterReduceBuffer[opi]
                   << ", 轮次=" << m_currentPass
                   << ", 数据块ID=" << logicalChunkToSend);
    }
  else  // m_currentPhase == ALL_GATHER
    {
      header.SetMessageType (ALL_GATHER_DATA);
      header.SetOriginalPacketIndex (opi);
      header.SetAggDataTest (m_scatterReduceBuffer[opi]);  // 此时值应为 m_numNodes
      header.SetPassNumber (m_currentPass);
      header.SetLogicalChunkIdentity (logicalChunkToSend);
      header.SetSenderNodeId (m_nodeId);
      header.SetCurrentPhase (static_cast<uint32_t>(m_currentPhase));
      
      NS_LOG_DEBUG ("节点 " << m_nodeId << " 发送ALL_GATHER_DATA: opi=" << opi 
                   << ", aggData=" << m_scatterReduceBuffer[opi]
                   << ", 轮次=" << m_currentPass
                   << ", 数据块ID=" << logicalChunkToSend);
    }
  
  // 创建数据包
  Ptr<Packet> packet = Create<Packet> (m_packetPayloadSize);
  packet->AddHeader (header);
  
  // 发送数据包
  int actualBytes = m_sendSocket->Send (packet);
  if (actualBytes > 0)
    {
      // 跟踪发送
      m_txTrace (packet);
      
      // 增加已发送包计数
      m_packetsSentForCurrentLogicalChunkInPass++;
      
      // 检查是否完成了当前逻辑数据块的发送
      if (m_packetsSentForCurrentLogicalChunkInPass < m_packetsPerChunk)
        {
          // 还有包待发送，安排下一次发送，使用设置的发包间隔
          m_sendEvent = Simulator::Schedule (MilliSeconds (m_packetInterval), &RingApplication::SendLoop, this);
        }
      else
        {
          NS_LOG_INFO ("节点 " << m_nodeId << " 在轮次 " << m_currentPass 
                      << " 中完成发送逻辑数据块 " << logicalChunkToSend);
          m_canSend = false;
          
          // 发送完成后，立即检查是否可以进入下一轮
          CheckAdvanceToNextRound();
        }
    }
  else
    {
      // 发送失败，等待一段时间后重试
      NS_LOG_WARN ("节点 " << m_nodeId << " 发送数据包失败，将重试");
      m_sendEvent = Simulator::Schedule (MilliSeconds (m_retryInterval), &RingApplication::SendLoop, this);
    }
}

uint32_t
RingApplication::CalculateLogicalChunkToSend (void) const
{
  NS_LOG_FUNCTION (this);
  
  uint32_t logicalChunkToSend;
  
  if (m_currentPhase == SCATTER_REDUCE)
    {
      // Scatter-Reduce阶段: chunk_to_send = (m_nodeId - k + m_numNodes) % m_numNodes
      logicalChunkToSend = (m_nodeId - m_currentPass + m_numNodes) % m_numNodes;
    }
  else  // m_currentPhase == ALL_GATHER
    {
      // All-Gather阶段: chunk_to_send = (m_nodeId - k+1 + m_numNodes) % m_numNodes
      logicalChunkToSend = (m_nodeId - m_currentPass + 1 + m_numNodes) % m_numNodes;
    }
  
  return logicalChunkToSend;
}

uint32_t
RingApplication::CalculateLogicalChunkToReceive (void) const
{
  NS_LOG_FUNCTION (this);
  
  // 当前轮次应接收的逻辑数据块索引为(发送块索引-1)%numNodes
  uint32_t logicalChunkToSend = CalculateLogicalChunkToSend ();
  uint32_t logicalChunkToReceive = (logicalChunkToSend + m_numNodes - 1) % m_numNodes;
  
  NS_LOG_DEBUG ("节点 " << m_nodeId << " 在轮次 " << m_currentPass 
               << " 应接收的数据块ID=" << logicalChunkToReceive);
  
  return logicalChunkToReceive;
}

void
RingApplication::CheckAdvanceToNextRound (void)
{
  NS_LOG_FUNCTION (this);
  
  // 检查是否发送完本轮数据
  bool sendingCompleted = (m_packetsSentForCurrentLogicalChunkInPass >= m_packetsPerChunk);
  
  // 检查是否接收完本轮应接收的数据块
  uint32_t logicalChunkToReceive = CalculateLogicalChunkToReceive ();
  bool receivingCompleted = (m_packetsReceivedForLogicalChunksInPass[logicalChunkToReceive] >= m_packetsPerChunk);
  
  NS_LOG_INFO ("节点 " << m_nodeId << " 检查轮次状态: 发送完成=" << sendingCompleted 
               << ", 接收完成=" << receivingCompleted);
  
  // 如果发送和接收都完成，通知前节点
  if (sendingCompleted && receivingCompleted && !m_hasNotifiedPreviousNode)
    {
      // 通知前节点本轮工作已完成
      SendRoundCompleteNotification(m_currentPass, m_currentPhase);
      m_hasNotifiedPreviousNode = true;
      
      // 立即进入下一轮的接收工作，无需等待后节点
      m_waitingForNextNode = true;
      AdvanceReceivingToNextRound();
    }

  
  // 检查发送和接收都完成，下一节点是否准备好，是否可以进入下一轮发送
  if (sendingCompleted && receivingCompleted && m_nextNodeState.readyForNextPass)
    {
      m_canSend = true;
      // 后节点已准备好，可以进入下一轮发送
      m_waitingForNextNode = false;
      AdvanceSendingToNextRound();
    }
}

void
RingApplication::AdvanceReceivingToNextRound (void)
{
  NS_LOG_FUNCTION (this);
  
  // 决定下一步操作
  if (m_currentPhase == SCATTER_REDUCE)
    {
      if (m_currentPass < m_numNodes - 2)
        {
          // 进入下一轮Scatter-Reduce
          AdvanceToNextPass ();
        }
      else
        {
          // 完成Scatter-Reduce阶段，进入All-Gather阶段
          AdvanceToNextPhase ();
        }
    }
  else if (m_currentPhase == ALL_GATHER)
    {
      if (m_currentPass < m_numNodes - 2)
        {
          // 进入下一轮All-Gather
          AdvanceToNextPass ();
        }
      else
        {
          // 完成All-Gather阶段，Ring Allreduce完成
          m_endTime = Simulator::Now ();
          m_currentPhase = DONE;
          
          // 在结束前，确保所有缓冲区都包含最终值
          for (uint32_t i = 0; i < m_totalPackets; ++i)
            {
              if (m_scatterReduceBuffer[i] == static_cast<int32_t>(m_numNodes))
                {
                  m_allGatherBuffer[i] = m_scatterReduceBuffer[i];
                }
            }
          
          NS_LOG_UNCOND ("节点 " << m_nodeId << " 完成Ring Allreduce，耗时 " 
                      << (m_endTime - m_startTime).GetSeconds () << " 秒");
          NS_LOG_UNCOND ("验证结果: " << (VerifyResults () ? "成功" : "失败"));
          
          // 停止应用
          StopApplication ();
        }
    }
}

void
RingApplication::AdvanceSendingToNextRound (void)
{
  NS_LOG_FUNCTION (this);
  
  // 设置发送就绪，允许发送，重置轮次通知状态
  m_nextNodeState.readyForNextPass = false;
  m_sendReady = true;
  m_canSend = true;
  
  // 开始发送数据
  SendLoop ();
}

void
RingApplication::SetTimingParams (double connectionStartTime, double transferStartTime)
{
  NS_LOG_FUNCTION (this << connectionStartTime << transferStartTime);
  
  m_connectionStartTime = connectionStartTime;
  m_transferStartTime = transferStartTime;
}

} // namespace ns3 