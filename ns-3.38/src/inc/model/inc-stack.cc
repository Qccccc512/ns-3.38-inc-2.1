/*
 * 在网计算协议 - 服务器端协议栈实现
 */

#include "inc-stack.h"
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "inc-header.h"
#include <string>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("IncStack");

NS_OBJECT_ENSURE_REGISTERED(IncStack);

TypeId
IncStack::GetTypeId()
{
  static TypeId tid =
      TypeId("ns3::IncStack")
          .SetParent<Application>()
          .SetGroupName("Applications")
          .AddConstructor<IncStack>()
          .AddAttribute("ServerId",
                        "服务器标识符",
                        StringValue(""),
                        MakeStringAccessor(&IncStack::m_serverId),
                        MakeStringChecker())
          .AddAttribute("GroupId",
                        "通信组ID",
                        UintegerValue(1),
                        MakeUintegerAccessor(&IncStack::m_groupId),
                        MakeUintegerChecker<uint16_t>())
          .AddAttribute("Interval",
                        "重传检查间隔",
                        TimeValue(MilliSeconds(20)),
                        MakeTimeAccessor(&IncStack::m_interval),
                        MakeTimeChecker())
          .AddAttribute("ProcessingDelay",
                        "报文处理时延",
                        TimeValue(MicroSeconds(10)),
                        MakeTimeAccessor(&IncStack::m_processingDelay),
                        MakeTimeChecker())
          .AddAttribute("LocalQP",
                        "本地QP号",
                        UintegerValue(1),
                        MakeUintegerAccessor(&IncStack::m_localQP),
                        MakeUintegerChecker<uint16_t>())
          .AddAttribute("RemoteQP",
                        "远程QP号",
                        UintegerValue(1),
                        MakeUintegerAccessor(&IncStack::m_remoteQP),
                        MakeUintegerChecker<uint16_t>())
          .AddAttribute("FillValue",
                        "填充值",
                        UintegerValue(1),
                        MakeUintegerAccessor(&IncStack::m_fillValue),
                        MakeUintegerChecker<uint32_t>())
          .AddAttribute("DataSize",
                        "发送数据大小(字节)",
                        UintegerValue(1024),
                        MakeUintegerAccessor(&IncStack::m_dataSize),
                        MakeUintegerChecker<uint32_t>())
          .AddAttribute("TotalPackets",
                        "发送数据包数目",
                        UintegerValue(3),
                        MakeUintegerAccessor(&IncStack::m_totalPackets),
                        MakeUintegerChecker<uint32_t>())    
          .AddAttribute("WindowSize",
                        "滑动窗口大小",
                        UintegerValue(16),
                        MakeUintegerAccessor(&IncStack::m_windowSize),
                        MakeUintegerChecker<uint16_t>())
          .AddTraceSource("Tx",
                        "发送数据包",
                        MakeTraceSourceAccessor(&IncStack::m_txTrace),
                        "ns3::Packet::TracedCallback")
          .AddTraceSource("Rx",
                        "接收数据包",
                        MakeTraceSourceAccessor(&IncStack::m_rxTrace),
                        "ns3::Packet::TracedCallback")
          .AddTraceSource("RxWithAddresses",
                        "接收数据包，包含地址信息",
                        MakeTraceSourceAccessor(&IncStack::m_rxTraceWithAddresses),
                        "ns3::Packet::AddressTracedCallback");
  return tid;
}

IncStack::IncStack()
    : m_groupId(1),
      m_operation(IncHeader::SUM),
      m_dataType(IncHeader::INT32),
      m_dataSize(1024),
      m_fillValue(1),
      m_windowSize(16),
      m_localQP(1),
      m_remoteQP(1),
      m_port(9),
      m_totalPackets(3),
      m_nextPsn(0),
      m_windowBase(0),
      m_windowEnd(0),
      m_recvSocket(nullptr),
      m_sendSocket(nullptr),
      m_running(false),
      m_allReduceStarted(false),
      m_allReduceCompleted(false),
      m_lastDataReceived(false)
{
  NS_LOG_FUNCTION(this);
}

IncStack::~IncStack()
{
  NS_LOG_FUNCTION(this);
}

void
IncStack::SetServerId(std::string id)
{
  NS_LOG_FUNCTION(this << id);
  m_serverId = id;
}

std::string
IncStack::GetServerId() const
{
  return m_serverId;
}

void
IncStack::SetGroupId(uint16_t groupId)
{
  NS_LOG_FUNCTION(this << groupId);
  m_groupId = groupId;
}

void
IncStack::SetOperation(IncHeader::Operation op)
{
  NS_LOG_FUNCTION(this << (int)op);
  m_operation = op;
}

void
IncStack::SetDataType(IncHeader::DataType dataType)
{
  NS_LOG_FUNCTION(this << (int)dataType);
  m_dataType = dataType;
}

void
IncStack::SetDataSize(uint32_t dataSize)
{
  NS_LOG_FUNCTION(this << dataSize);
  m_dataSize = dataSize;
}

void
IncStack::SetFillValue(uint32_t value)
{
  NS_LOG_FUNCTION(this << value);
  m_fillValue = value;
}

void
IncStack::SetWindowSize(uint16_t windowSize)
{
  NS_LOG_FUNCTION(this << windowSize);
  m_windowSize = windowSize;
}

void
IncStack::SetRemote(Ipv4Address remoteAddr, uint16_t remoteQP)
{
  NS_LOG_FUNCTION(this << remoteAddr << remoteQP);
  m_remoteAddr = remoteAddr;
  m_remoteQP = remoteQP;
}

void
IncStack::SetLocal(Ipv4Address localAddr, uint16_t localQP)
{
  NS_LOG_FUNCTION(this << localAddr << localQP);
  m_localAddr = localAddr;
  m_localQP = localQP;
}

const std::vector<int32_t>&
IncStack::GetResultBuffer() const
{
  return m_recvBuffer;
}

void
IncStack::DoDispose()
{
  NS_LOG_FUNCTION(this);
  m_recvSocket = nullptr;
  m_sendSocket = nullptr;
  
  // 取消所有事件
  if (m_sendEvent.IsRunning())
  {
    m_sendEvent.Cancel();
  }
  
  // 取消循环发送事件
  if (m_circleSendEvent.IsRunning())
  {
    m_circleSendEvent.Cancel();
  }
  
  // 取消所有报文重传事件
  for (auto it = m_retransmitEvents.begin(); it != m_retransmitEvents.end(); ++it)
  {
    if (it->second.IsRunning())
    {
      it->second.Cancel();
    }
  }
  m_retransmitEvents.clear();
  
  Application::DoDispose();
}

void
IncStack::StartApplication()
{
  NS_LOG_FUNCTION(this);
  
  // 创建接收Socket
  if (m_recvSocket == nullptr)
  {
    // 创建UDP Socket
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_recvSocket = Socket::CreateSocket(GetNode(), tid);
    
    // 绑定到本地地址和固定端口9
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    
    // 绑定并设置接收回调
    if (m_recvSocket->Bind(local) == -1)
    {
      NS_FATAL_ERROR("接收Socket绑定失败");
    }
    m_recvSocket->SetRecvCallback(MakeCallback(&IncStack::HandleRead, this));
  }
  
  // 创建发送Socket
  if (m_sendSocket == nullptr)
  {
    // 创建UDP Socket
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_sendSocket = Socket::CreateSocket(GetNode(), tid);
    
    // 绑定到本地特定地址和端口(QP+1024)
    uint16_t localPort = m_localQP + 1024;
    InetSocketAddress local = InetSocketAddress(m_localAddr, localPort);
    if (m_sendSocket->Bind(local) == -1)
    {
      NS_FATAL_ERROR("发送Socket绑定失败");
    }
    
    // 连接到远程地址和端口9
    m_sendSocket->Connect(InetSocketAddress(m_remoteAddr, m_port));
  }
  
  m_running = true;
}

void
IncStack::StopApplication()
{
  NS_LOG_FUNCTION(this);
  m_running = false;

  if (m_recvSocket != nullptr)
  {
    m_recvSocket->Close();
    m_recvSocket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    m_recvSocket = nullptr;
  }
  
  if (m_sendSocket != nullptr)
  {
    m_sendSocket->Close();
    m_sendSocket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    m_sendSocket = nullptr;
  }
  
  // 取消所有事件
  if (m_sendEvent.IsRunning())
  {
    m_sendEvent.Cancel();
  }
  
  // 取消循环发送事件
  if (m_circleSendEvent.IsRunning())
  {
    m_circleSendEvent.Cancel();
  }
  
  // 取消所有报文重传事件
  for (auto it = m_retransmitEvents.begin(); it != m_retransmitEvents.end(); ++it)
  {
    if (it->second.IsRunning())
    {
      it->second.Cancel();
    }
  }
  m_retransmitEvents.clear();
}

void
IncStack::SetTotalPackets(uint32_t totalPackets)
{
  NS_LOG_FUNCTION(this << totalPackets);
  m_totalPackets = totalPackets;
}

void
IncStack::SetCompleteCallback(CompleteCallback callback)
{
  NS_LOG_FUNCTION(this);
  m_completeCallback = callback;
}

bool
IncStack::IsCompleted() const
{
  return m_allReduceCompleted;
}

void
IncStack::AllReduce()
{
  NS_LOG_FUNCTION(this);
  
  if (!m_running || m_allReduceStarted)
  {
    NS_LOG_WARN(m_serverId << ": 无法启动AllReduce，协议栈未运行或已有运行中的AllReduce");
    return;
  }
  
  NS_LOG_INFO(m_serverId << ": 启动AllReduce操作");
  m_allReduceStarted = true;
  m_allReduceCompleted = false;
  m_lastDataReceived = false;
  
  // -只有在未设置总报文数时才计算
  if (m_totalPackets == 0)
  {
    // 计算总报文数量，每个报文载荷固定为1024B
    m_totalPackets = m_dataSize / 1024;
    if (m_dataSize % 1024 != 0)
    {
      m_totalPackets++;
    }
  }
  
  // 初始化发送缓冲区
  m_sendBuffer.resize(m_totalPackets, m_fillValue);
  
  // 初始化接收缓冲区
  m_recvBuffer.resize(m_totalPackets, 0);
  
  // 初始化状态数组
  m_ackReceived.resize(m_totalPackets, false);
  m_dataReceived.resize(m_totalPackets, false);
  m_inFlight.resize(m_totalPackets, false);
  
  // 清空重传事件映射
  for (auto it = m_retransmitEvents.begin(); it != m_retransmitEvents.end(); ++it)
  {
    if (it->second.IsRunning())
    {
      it->second.Cancel();
    }
  }
  m_retransmitEvents.clear();
  
  // 设置窗口
  m_nextPsn = 0;
  m_windowBase = 0;
  m_windowEnd = std::min(m_windowBase + m_windowSize - 1, m_totalPackets - 1);
  
  // 开始发送数据
  NS_LOG_INFO(m_serverId << ": 开始发送数据，总报文数=" << m_totalPackets);
  SendWindowData();
}

void
IncStack::HandleRead(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this << socket);
  Ptr<Packet> packet;
  Address from;
  
  while ((packet = socket->RecvFrom(from)))
  {
    //NS_LOG_INFO(m_serverId << ": 接收到数据包，包体大小=" << packet->GetSize());
    m_rxTrace(packet);
    m_rxTraceWithAddresses(packet, from);
    
    // 获取并移除头部
    IncHeader header;
    packet->RemoveHeader(header);
    
    if (header.HasFlag(IncHeader::ACK))
    {
      NS_LOG_INFO(m_serverId << ": 接收到ACK报文 PSN=" << header.GetPsn());
      ProcessAckPacket(packet, header);
    }
    else if (header.HasFlag(IncHeader::NACK))
    {
      NS_LOG_INFO(m_serverId << ": 接收到NAK报文 PSN=" << header.GetPsn());
      ProcessNakPacket(packet, header);
    }
    else
    {
      NS_LOG_INFO(m_serverId << ": 接收到数据报文 PSN=" << header.GetPsn() 
                  << " agg_data_test=" << header.GetAggDataTest());
      ProcessDataPacket(packet, header);
    }
    
    // 检查AllReduce是否完成
    if (m_allReduceStarted && !m_allReduceCompleted && IsAllReduceComplete())
    {
      NS_LOG_INFO(m_serverId << ": AllReduce操作完成");
      m_allReduceCompleted = true;
      
      // 调用完成回调
      if (!m_completeCallback.IsNull())
      {
        NS_LOG_INFO(m_serverId << ": 触发完成回调");
        m_completeCallback();
      }
    }
  }
}

void
IncStack::SendData(uint32_t psn)
{
  NS_LOG_FUNCTION(this << psn);
  
  if (psn >= m_totalPackets || !m_running)
  {
    NS_LOG_WARN(m_serverId << ": 尝试发送超出范围的报文 PSN=" << psn);
    return;
  }
  
  // 创建要发送的数据报文，使用ns3 packet的默认构造，仅用来填充数据包至预期大小
  Ptr<Packet> packet = Create<Packet>(1024); // 默认包大小1024B
  
  // 创建头部
  IncHeader header;
  header.SetSrcAddr(m_localAddr);
  header.SetDstAddr(m_remoteAddr);
  header.SetSrcQP(m_localQP);
  header.SetDstQP(m_remoteQP);
  header.SetPsn(psn);
  header.SetOperation(m_operation);
  header.SetDataType(m_dataType);
  header.SetGroupId(m_groupId);
  header.SetLength(header.GetSerializedSize() + 1024); // 头部大小 + 载荷大小
  
  // 设置agg_data_test字段，用m_sendBuffer中的值
  header.SetAggDataTest(m_sendBuffer[psn]);
  
  // 添加头部
  packet->AddHeader(header);
  
  // 直接使用sendSocket发送，不需要重复bind和connect
  m_sendSocket->Send(packet);
  
  NS_LOG_INFO(m_serverId << ": 发送数据报文 PSN=" << psn 
              << " agg_data_test=" << m_sendBuffer[psn]
              << " 到 " << m_remoteAddr << " QP=" << m_remoteQP);
  
  m_txTrace(packet);
}

void
IncStack::SendWindowData()
{
  NS_LOG_FUNCTION(this);
  
  // 仅设置窗口并启动CircleSend
  m_nextPsn = m_windowBase;
  
  // 如果循环发送事件未在运行，启动它
  if (!m_circleSendEvent.IsRunning()) {
    m_circleSendEvent = Simulator::Schedule(Seconds(0), &IncStack::CircleSend, this);
  }
}

void
IncStack::CircleSend()
{
  NS_LOG_FUNCTION(this);
  
  if (!m_running) {
    return;
  }
  
  // 检查nextpsn是否在窗口内
  if (m_nextPsn >= m_windowBase && m_nextPsn <= m_windowEnd && m_nextPsn < m_totalPackets) {
    // 仅发送未收到ACK且非在途的报文
    if (!m_ackReceived[m_nextPsn] && !m_inFlight[m_nextPsn]) {
      // 标记报文为传输中
      m_inFlight[m_nextPsn] = true;
      
      // 发送数据
      SendData(m_nextPsn);
      
      // 设置报文重传计时器
      EventId retransmitEvent = Simulator::Schedule(m_interval, &IncStack::RetransmitPacket, this, m_nextPsn);
      
      // 保存重传事件
      auto it = m_retransmitEvents.find(m_nextPsn);
      if (it != m_retransmitEvents.end()) {
        // 如果已经存在重传事件，先取消它
        if (it->second.IsRunning()) {
          it->second.Cancel();
        }
        it->second = retransmitEvent;
      } else {
        // 添加新的重传事件
        m_retransmitEvents[m_nextPsn] = retransmitEvent;
      }
      
      // 递增下一个PSN
      m_nextPsn++;
    } else {
      // 已收到ACK或在途的报文，跳过
      m_nextPsn++;
    }
    
    // 在处理延迟后继续循环发送
    m_circleSendEvent = Simulator::Schedule(m_processingDelay, &IncStack::CircleSend, this);
  } 
  else if (m_nextPsn < m_totalPackets) {
    // m_nextPsn超出窗口但未超出总报文数，等待窗口移动
    m_circleSendEvent = Simulator::Schedule(m_processingDelay, &IncStack::CircleSend, this);
  }
  // 如果m_nextPsn >= m_totalPackets，循环停止
}

void
IncStack::ScheduleSendPacket(uint32_t psn)
{
  NS_LOG_FUNCTION(this << psn);
  
  if (psn >= m_totalPackets)
  {
    NS_LOG_WARN(m_serverId << ": 尝试调度超出范围的报文 PSN=" << psn);
    return;
  }
  
  if (!m_running || m_ackReceived[psn])
  {
    return;
  }
  
  // 标记报文为传输中
  m_inFlight[psn] = true;
  
  // 直接发送数据，不再使用延迟
  SendData(psn);
  
  // 设置报文重传计时器
  EventId retransmitEvent = Simulator::Schedule(m_interval, &IncStack::RetransmitPacket, this, psn);
  
  // 保存重传事件
  auto it = m_retransmitEvents.find(psn);
  if (it != m_retransmitEvents.end())
  {
    // 如果已经存在重传事件，先取消它
    if (it->second.IsRunning())
    {
      it->second.Cancel();
    }
    it->second = retransmitEvent;
  }
  else
  {
    // 添加新的重传事件
    m_retransmitEvents[psn] = retransmitEvent;
  }
  
  NS_LOG_INFO(m_serverId << ": 调度发送报文 PSN=" << psn);
}

void
IncStack::RetransmitPacket(uint32_t psn)
{
  NS_LOG_FUNCTION(this << psn);
  
  if (psn >= m_totalPackets || !m_running || m_ackReceived[psn])
  {
    return;
  }
  
  NS_LOG_INFO(m_serverId << ": 准备重传报文 PSN=" << psn);
  
  // 标记报文为传输中
  m_inFlight[psn] = true;
  
  // 发送数据
  Simulator::Schedule(m_processingDelay, &IncStack::SendData, this, psn);
  
  // 设置下一次重传事件
  EventId nextRetransmit = Simulator::Schedule(m_interval, &IncStack::RetransmitPacket, this, psn);
  
  // 更新重传事件
  auto it = m_retransmitEvents.find(psn);
  if (it != m_retransmitEvents.end())
  {
    it->second = nextRetransmit;
  }
  else
  {
    m_retransmitEvents[psn] = nextRetransmit;
  }
}

void
IncStack::ProcessDataPacket(Ptr<Packet> packet, const IncHeader& header)
{
  NS_LOG_FUNCTION(this);
  
  // 检查是否在报文范围内
  uint32_t psn = header.GetPsn();
  if (psn >= m_totalPackets)
  {
    NS_LOG_WARN(m_serverId << ": 接收到超出范围的数据报文 PSN=" << psn);
    return;
  }
  
  // 如果是重复报文，仍然回复ACK但不处理数据
  if (m_dataReceived[psn])
  {
    NS_LOG_INFO(m_serverId << ": 接收到重复数据报文 PSN=" << psn);
    SendAck(header, header.GetAggDataTest());
    return;
  }
  
  // 获取agg_data_test字段的值
  int32_t aggDataTest = header.GetAggDataTest();
  
  // 记录接收到的聚合测试数据
  m_recvBuffer[psn] = aggDataTest;
  m_dataReceived[psn] = true;
  
  // 检查是否是最后一个数据包
  if (psn == m_totalPackets - 1)
  {
    m_lastDataReceived = true;
  }
  
  NS_LOG_INFO(m_serverId << ": 接收到数据 PSN=" << psn << " agg_data_test=" << aggDataTest);
  
  // 回复ACK，将原始agg_data_test值传递回去
  SendAck(header, aggDataTest);
}

void
IncStack::ProcessAckPacket(Ptr<Packet> packet, const IncHeader& header)
{
  NS_LOG_FUNCTION(this);
  
  uint32_t psn = header.GetPsn();
  
  // 检查是否是有效PSN
  if (psn >= m_totalPackets)
  {
    NS_LOG_WARN(m_serverId << ": 接收到超出范围的ACK报文 PSN=" << psn);
    return;
  }
  
  // 记录ACK状态
  m_ackReceived[psn] = true;
  
  // 标记报文不再传输中
  m_inFlight[psn] = false;
  
  // 取消该报文的重传计时器
  auto it = m_retransmitEvents.find(psn);
  if (it != m_retransmitEvents.end())
  {
    if (it->second.IsRunning())
    {
      it->second.Cancel();
    }
    m_retransmitEvents.erase(it);
  }
  
  // 检查是否可以移动窗口
  while (m_ackReceived[m_windowBase] && m_windowBase < m_totalPackets)
  {
    m_windowBase++;
    
    // 如果窗口结束未到达总报文数，则扩展窗口
    if (m_windowEnd < m_totalPackets - 1)
    {
      m_windowEnd++;
    }
  }
  
  NS_LOG_INFO(m_serverId << ": 处理ACK PSN=" << psn 
             << " 窗口基址=" << m_windowBase 
             << " 窗口结束=" << m_windowEnd);
  
  // 不再立即调度发送新报文，CircleSend会自行检查并发送
}

void
IncStack::ProcessNakPacket(Ptr<Packet> packet, const IncHeader& header)
{
  NS_LOG_FUNCTION(this);
  
  uint32_t psn = header.GetPsn();
  
  // 检查是否是有效PSN
  if (psn >= m_totalPackets)
  {
    NS_LOG_WARN(m_serverId << ": 接收到超出范围的NAK报文 PSN=" << psn);
    return;
  }
  
  // 立即重传请求的数据包
  NS_LOG_INFO(m_serverId << ": 收到NAK，重传数据包 PSN=" << psn);
  // 使用ScheduleSendPacket而非直接SendData，确保状态和重传事件的一致性
  ScheduleSendPacket(psn);
}

void
IncStack::SendAck(const IncHeader& header, int32_t aggDataTest)
{
  NS_LOG_FUNCTION(this);
  
  // 获取源数据包信息
  Ipv4Address srcAddr = header.GetSrcAddr();
  Ipv4Address dstAddr = header.GetDstAddr();
  uint16_t srcQP = header.GetSrcQP();
  uint16_t dstQP = header.GetDstQP();
  uint32_t psn = header.GetPsn();
  
  // 创建ACK包
  Ptr<Packet> ackPacket = Create<Packet>(0); // 空载荷
  
  // 创建ACK头部
  IncHeader ackHeader;
  ackHeader.SetSrcAddr(dstAddr);     // 交换源目地址
  ackHeader.SetDstAddr(srcAddr);
  ackHeader.SetSrcQP(dstQP);         // 交换源目QP
  ackHeader.SetDstQP(srcQP);
  ackHeader.SetPsn(psn);
  ackHeader.SetOperation(header.GetOperation());
  ackHeader.SetDataType(header.GetDataType());
  ackHeader.SetFlag(IncHeader::ACK);
  ackHeader.SetGroupId(header.GetGroupId());
  ackHeader.SetLength(ackHeader.GetSerializedSize());
  
  // 设置ACK报文的agg_data_test字段与其回应的数据报文的agg_data_test字段相同
  ackHeader.SetAggDataTest(aggDataTest);
  
  // 添加头部
  ackPacket->AddHeader(ackHeader);
  
  // 直接使用sendSocket发送，临时更改目标地址
  m_sendSocket->Connect(InetSocketAddress(srcAddr, m_port));
  m_sendSocket->Send(ackPacket);
  
  // 重新连接到默认目标地址，以便后续发送数据
  m_sendSocket->Connect(InetSocketAddress(m_remoteAddr, m_port));
  // 事实上不必要，两次连接的对象是同一个
  
  NS_LOG_INFO(m_serverId << ": 发送ACK PSN=" << psn 
              << " agg_data_test=" << aggDataTest
              << " 到 " << srcAddr << " QP=" << srcQP);
}

bool
IncStack::IsAllReduceComplete()
{
  NS_LOG_FUNCTION(this);
  
  // 如果最后一个数据包已接收并且最后一个数据包的ACK已收到
  if (m_lastDataReceived && m_ackReceived[m_totalPackets - 1])
  {
    // 清理所有重传事件，避免资源泄漏
    for (auto it = m_retransmitEvents.begin(); it != m_retransmitEvents.end(); ++it)
    {
      if (it->second.IsRunning())
      {
        it->second.Cancel();
      }
    }
    m_retransmitEvents.clear();
    
    return true;
  }
  
  return false;
}

} // namespace ns3 