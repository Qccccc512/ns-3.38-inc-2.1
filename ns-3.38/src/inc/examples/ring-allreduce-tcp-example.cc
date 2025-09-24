/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ring-application.h"
#include "ns3/error-model.h"
#include "ns3/config.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingAllreduceTcpExample");

int
main (int argc, char *argv[])
{
  // 日志设置
  LogComponentEnable ("RingAllreduceTcpExample", LOG_LEVEL_WARN);
  LogComponentEnable ("RingApplication", LOG_LEVEL_ERROR);
  //LogComponentEnable ("RingHeader", LOG_LEVEL_INFO);

  // 默认参数
  uint32_t nNodes = 4;              // 节点数量
  uint32_t totalPackets = 16; //1024 * 128;        // 每个节点的数据包数 (必须能被nNodes整除)
  uint32_t packetSize = 1024;       // 数据包大小 (字节)
  uint32_t rcwndSize = 1024 * 1024 * 2;   // TCP接收窗口大小
  double linkRate = 1000000000;     // 链路速率 (bps)
  double linkDelay = 0.001;         // 链路延迟 (秒)
  double errorRate = 0;          // 链路错误率
  double simulationTime = 10000.0;     // 仿真时间 (秒)
  uint32_t mtu = 1064;              // MTU大小 (默认1500字节，大于我们的数据包大小)
  uint32_t checkInterval = 10;      // 状态检查间隔(毫秒)
  double connectionStartTime = 1.0;  // 默认在模拟开始1秒后开始建立连接
  double transferStartTime = 5.0;    // 默认在模拟开始5秒后开始数据传输
  uint32_t retryInterval = 5;       // 默认重试间隔为5毫秒
  double packetInterval = 0.01;     // 默认发包间隔为0.01毫秒

  // 命令行参数
  CommandLine cmd (__FILE__);
  cmd.AddValue ("nNodes", "节点数量", nNodes);
  cmd.AddValue ("totalPackets", "每个节点的总数据包数", totalPackets);
  cmd.AddValue ("packetSize", "数据包大小（字节）", packetSize);
  cmd.AddValue ("rcwndSize", "TCP接收窗口大小", rcwndSize);
  cmd.AddValue ("linkRate", "点对点链路速率（bps）", linkRate);
  cmd.AddValue ("linkDelay", "点对点链路延迟（秒）", linkDelay);
  cmd.AddValue ("errorRate", "链路错误率", errorRate);
  cmd.AddValue ("simulationTime", "仿真时间（秒）", simulationTime);
  cmd.AddValue ("mtu", "链路MTU大小（字节）", mtu);
  cmd.AddValue ("checkInterval", "状态检查间隔(毫秒)", checkInterval);
  cmd.AddValue ("connectionTime", "连接建立时间(秒)", connectionStartTime);
  cmd.AddValue ("transferTime", "数据传输开始时间(秒)", transferStartTime);
  cmd.AddValue ("retryInterval", "重试发送间隔(毫秒)", retryInterval);
  cmd.AddValue ("packetInterval", "发包间隔时间(毫秒)", packetInterval);
  cmd.Parse (argc, argv);

  // 设置TCP最小重传超时时间(ms)
  //Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (retryInterval)));

  // 设置TCP初始RTO值(ms)
  //Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue (MilliSeconds (retryInterval)));

  // 设置TCP连接超时时间
  //Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue (Seconds (10.0)));

  // 参数检查
  if (totalPackets % nNodes != 0)
    {
      NS_FATAL_ERROR ("总数据包数必须能被节点数整除");
    }

  // 设置TCP相关参数，防止分段
  // 设置TCP段大小与MTU相关的参数
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (mtu - 40)); // MTU减去IP和TCP头部大小
  
  // 创建节点
  NS_LOG_INFO ("创建 " << nNodes << " 个节点组成环形拓扑");
  NodeContainer nodes;
  nodes.Create (nNodes);

  // 配置点对点链路
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (linkRate)));
  p2p.SetChannelAttribute ("Delay", TimeValue (Seconds (linkDelay)));
  // 设置MTU
  p2p.SetDeviceAttribute ("Mtu", UintegerValue (mtu));
  
  // 创建错误模型
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (errorRate));
  em->SetAttribute ("ErrorUnit", EnumValue (RateErrorModel::ERROR_UNIT_PACKET));
  
  // 创建网络设备并应用错误模型
  NetDeviceContainer devices[nNodes];
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      uint32_t next = (i + 1) % nNodes;
      devices[i] = p2p.Install (NodeContainer (nodes.Get (i), nodes.Get (next)));
      
      // 在两个方向上都应用错误模型
      devices[i].Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
      devices[i].Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
    }

  // 安装Internet协议栈
  InternetStackHelper internet;
  internet.Install (nodes);

  // 分配IP地址
  Ipv4AddressHelper ipv4;
  Ipv4InterfaceContainer interfaces[nNodes];
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i + 1 << ".0";
      ipv4.SetBase (subnet.str().c_str(), "255.255.255.0");
      interfaces[i] = ipv4.Assign (devices[i]);
    }

  // 打印节点分配的IP地址和MTU
  NS_LOG_INFO ("节点IP分配:");
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      NS_LOG_INFO ("节点 " << i << ": " << interfaces[i].GetAddress (0) 
                   << " MTU: " << devices[i].Get(0)->GetMtu());
    }

  // 创建并配置应用程序
  std::vector<Ptr<RingApplication>> apps;
  for (uint32_t i = 0; i < nNodes; ++i) {
    Ptr<RingApplication> app = CreateObject<RingApplication> ();
    app->SetAttribute ("NodeId", UintegerValue (i));
    app->SetAttribute ("NumNodes", UintegerValue (nNodes));
    app->SetAttribute ("TotalPackets", UintegerValue (totalPackets));
    app->SetAttribute ("PacketInterval", DoubleValue (packetInterval));
    
    // 设置监听地址和端口
    app->SetListenConfig (interfaces[i].GetAddress (0), 9000);
    
    // 设置对等节点地址和端口
    uint32_t peerIndex = (i + 1) % nNodes;
    app->SetPeer (interfaces[peerIndex].GetAddress (0), 9000);
    
    // 设置应用参数，包括新添加的发包间隔参数
    app->Setup (i, nNodes, totalPackets, packetSize, rcwndSize, checkInterval, retryInterval, 
                connectionStartTime, transferStartTime, packetInterval);
    
    nodes.Get(i)->AddApplication (app);
    app->SetStartTime (Seconds (0.0));
    app->SetStopTime (Seconds (simulationTime));
    
    apps.push_back (app);
  }

  // 启用路由
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  // 运行仿真
  NS_LOG_INFO ("运行仿真 " << simulationTime << " 秒");
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

    // 验证结果
  bool allSucceeded = true;
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      Ptr<RingApplication> app = DynamicCast<RingApplication> (apps[i]);
      if (!app->VerifyResults ())
        {
          allSucceeded = false;
          NS_LOG_ERROR ("节点 " << i << " 验证失败!");
        }
    }

  if (allSucceeded)
    {
      NS_LOG_UNCOND ("所有节点验证成功!");
    }
  else
    {
      NS_LOG_UNCOND ("有节点验证失败!");
    }

  // 输出总体模拟耗时
  NS_LOG_UNCOND ("Ring Allreduce模拟完成");
  NS_LOG_UNCOND ("设置参数: 节点数=" << nNodes << ", 错误率=" << errorRate 
                << ", 重试间隔=" << retryInterval << "ms, 数据包数=" << totalPackets
                << ", 发包间隔=" << packetInterval << "ms");
                
  Simulator::Destroy ();


  return 0;
} 