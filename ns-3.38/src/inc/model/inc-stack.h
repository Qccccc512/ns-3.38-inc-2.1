/*
 * 在网计算协议 - 服务器端协议栈
 */

#ifndef INC_STACK_H
#define INC_STACK_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/address.h"
#include "ns3/traced-callback.h"
#include "ns3/string.h"
#include "ns3/ipv4-address.h"
#include "ns3/socket.h"
#include "ns3/packet.h"
#include <vector>
#include <map>
#include "inc-header.h"
#include "ns3/callback.h"

namespace ns3
{

/**
 * \brief 在网计算协议服务器端协议栈
 *
 * 实现协议中定义的服务器端功能，支持AllReduce原语
 */
class IncStack : public Application
{
public:
  /**
   * \brief 获取类型ID
   * \return 对象TypeId
   */
  static TypeId GetTypeId();
  IncStack();
  ~IncStack() override;

  /**
   * \brief 设置服务器ID
   * \param id 服务器ID
   */
  void SetServerId(std::string id);

  /**
   * \brief 获取服务器ID
   * \return 服务器ID
   */
  std::string GetServerId() const;

  /**
   * \brief 设置通信组ID
   * \param groupId 通信组ID
   */
  void SetGroupId(uint16_t groupId);

  /**
   * \brief 设置操作类型
   * \param op 操作类型
   */
  void SetOperation(IncHeader::Operation op);

  /**
   * \brief 设置数据类型
   * \param dataType 数据类型
   */
  void SetDataType(IncHeader::DataType dataType);

  /**
   * \brief 设置要发送的数据大小
   * \param dataSize 数据大小(字节)
   */
  void SetDataSize(uint32_t dataSize);

  /**
   * \brief 设置填充数据的值(int32_t)
   * \param value 要填充的值
   */
  void SetFillValue(uint32_t value);

  /**
   * \brief 设置滑动窗口大小
   * \param windowSize 窗口大小
   */
  void SetWindowSize(uint16_t windowSize);

  /**
   * \brief 设置远程连接信息
   * \param remoteAddr 远程IP地址
   * \param remoteQP 远程QP号
   */
  void SetRemote(Ipv4Address remoteAddr, uint16_t remoteQP);

  /**
   * \brief 设置本地连接信息
   * \param localAddr 本地IP地址
   * \param localQP 本地QP号
   */
  void SetLocal(Ipv4Address localAddr, uint16_t localQP);

  /**
   * \brief 获取接收到的结果缓冲区
   * \return 结果缓冲区的引用
   */
  const std::vector<int32_t>& GetResultBuffer() const;

  /**
   * \brief 执行AllReduce操作
   */
  void AllReduce();

  /**
   * \brief 设置总数据包数
   * \param totalPackets 要发送的总数据包数
   */
  void SetTotalPackets(uint32_t totalPackets);

  /**
   * \brief 回调函数类型定义，用于AllReduce操作完成通知
   */
  typedef Callback<void> CompleteCallback;

  /**
   * \brief 设置AllReduce完成回调函数
   * \param callback 完成时调用的回调函数
   */
  void SetCompleteCallback(CompleteCallback callback);

  /**
   * \brief 检查AllReduce操作是否已完成
   * \return 如果完成返回true，否则返回false
   */
  bool IsCompleted() const;

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
   * \brief 发送数据报文
   * \param psn 序列号
   */
  void SendData(uint32_t psn);

  /**
   * \brief 发送窗口内的数据
   */
  void SendWindowData();

  /**
   * \brief 发送单个新进入窗口的报文
   * \param psn 报文序列号
   */
  void ScheduleSendPacket(uint32_t psn);

  /**
   * \brief 循环发送窗口内的数据
   * 检查nextpsn是否在窗口内，如果在则发送并增加nextpsn，否则等待
   */
  void CircleSend();

  /**
   * \brief 处理收到的数据报文
   * \param packet 收到的数据包
   * \param header 解析出的IncHeader
   */
  void ProcessDataPacket(Ptr<Packet> packet, const IncHeader& header);

  /**
   * \brief 处理收到的ACK报文
   * \param packet 收到的数据包
   * \param header 解析出的IncHeader
   */
  void ProcessAckPacket(Ptr<Packet> packet, const IncHeader& header);

  /**
   * \brief 处理收到的NAK报文
   * \param packet 收到的数据包
   * \param header 解析出的IncHeader
   */
  void ProcessNakPacket(Ptr<Packet> packet, const IncHeader& header);

  /**
   * \brief 发送ACK确认
   * \param header 原始数据包的头部信息
   * \param aggDataTest 聚合测试数据值
   */
  void SendAck(const IncHeader& header, int32_t aggDataTest);

  /**
   * \brief 检查AllReduce是否完成
   * \return 如果完成返回true，否则返回false
   */
  bool IsAllReduceComplete();

  /**
   * \brief 重传特定序列号的数据包
   * \param psn 需要重传的序列号
   */
  void RetransmitPacket(uint32_t psn);

  std::string m_serverId;             //!< 服务器标识符
  uint16_t m_groupId;                 //!< 通信组ID
  IncHeader::Operation m_operation;   //!< 操作类型
  IncHeader::DataType m_dataType;     //!< 数据类型
  uint32_t m_dataSize;                //!< 数据大小(字节)
  uint32_t m_fillValue;               //!< 填充值
  uint16_t m_windowSize;              //!< 滑动窗口大小

  Ipv4Address m_localAddr;            //!< 本地IP地址
  uint16_t m_localQP;                 //!< 本地QP号
  Ipv4Address m_remoteAddr;           //!< 远程IP地址
  uint16_t m_remoteQP;                //!< 远程QP号
  uint16_t m_port;                    //!< 本地监听端口(固定为9)

  std::vector<int32_t> m_sendBuffer;  //!< 发送缓冲区，存储agg_data_test值
  std::vector<int32_t> m_recvBuffer;  //!< 接收缓冲区，存储agg_data_test值
  std::vector<bool> m_ackReceived;    //!< ACK接收状态
  std::vector<bool> m_dataReceived;   //!< 数据接收状态
  std::vector<bool> m_inFlight;       //!< 标记报文是否在传输中

  uint32_t m_totalPackets;            //!< 总报文数
  uint32_t m_nextPsn;                 //!< 下一个发送的序列号
  uint32_t m_windowBase;              //!< 当前窗口的基础位置
  uint32_t m_windowEnd;               //!< 当前窗口的结束位置

  Ptr<Socket> m_recvSocket;           //!< 接收数据的UDP Socket
  Ptr<Socket> m_sendSocket;           //!< 发送数据的UDP Socket
  Address m_local;                    //!< 本地绑定地址

  EventId m_sendEvent;                //!< 发送事件
  EventId m_circleSendEvent;          //!< 循环发送事件
  Time m_interval;                    //!< 重传间隔
  Time m_processingDelay;             //!< 处理时延
  std::map<uint32_t, EventId> m_retransmitEvents; //!< 报文重传事件映射

  bool m_running;                     //!< 是否正在运行
  bool m_allReduceStarted;            //!< AllReduce是否已启动
  bool m_allReduceCompleted;          //!< AllReduce是否已完成
  bool m_lastDataReceived;            //!< 是否接收到最后一个数据包

  // 跟踪回调
  TracedCallback<Ptr<const Packet>> m_txTrace;
  TracedCallback<Ptr<const Packet>> m_rxTrace;
  TracedCallback<Ptr<const Packet>, const Address&> m_rxTraceWithAddresses;

  CompleteCallback m_completeCallback;  //!< AllReduce完成回调
};

} // namespace ns3

#endif /* INC_STACK_H */ 