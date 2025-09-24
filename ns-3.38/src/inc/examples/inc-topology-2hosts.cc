/*
 * 在网计算协议 - 模拟测试：一个交换机连接两个主机
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/csma-module.h"
#include "ns3/error-model.h"
#include "../model/inc-stack.h"
#include "../model/inc-switch.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("IncTwoHostsOneSwitch");

// 回调函数，用于在AllReduce完成时通知
void AllReduceCompletionCallback(std::string id)
{
  NS_LOG_UNCOND("时间 " << Simulator::Now().GetSeconds() << "s: 主机 " << id << " 完成 AllReduce 操作");
}

int
main(int argc, char* argv[])
{
  // 命令行参数处理
  double errorRate = 0.0;  // 默认错误率10%
  CommandLine cmd(__FILE__);
  cmd.AddValue("error", "Error rate for channels", errorRate);
  cmd.Parse(argc, argv);

  // 日志组件配置
  LogComponentEnable("IncTwoHostsOneSwitch", LOG_LEVEL_INFO);
  LogComponentEnable("IncStack", LOG_LEVEL_WARN);
  LogComponentEnable("IncSwitch", LOG_LEVEL_WARN);

  // 创建两个节点用作主机
  NodeContainer hosts;
  hosts.Create(2);

  // 创建一个节点用作交换机
  NodeContainer switchNode;
  switchNode.Create(1);

  // 创建用于连接主机和交换机的链路
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Gbps"));  // 1Gbps链路
  pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));      // 1ms延迟

  // 创建错误模型
  Ptr<RateErrorModel> em1 = CreateObject<RateErrorModel>();
  em1->SetAttribute("ErrorRate", DoubleValue(errorRate));
  em1->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
  
  Ptr<RateErrorModel> em2 = CreateObject<RateErrorModel>();
  em2->SetAttribute("ErrorRate", DoubleValue(errorRate));
  em2->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

  // 连接Host0和Switch
  NetDeviceContainer devicesH0S;
  devicesH0S = pointToPoint.Install(hosts.Get(0), switchNode.Get(0));
  
  // 应用错误模型到设备上
  devicesH0S.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em1));
  devicesH0S.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em1));

  // 连接Host1和Switch
  NetDeviceContainer devicesH1S;
  devicesH1S = pointToPoint.Install(hosts.Get(1), switchNode.Get(0));
  
  // 应用错误模型到设备上
  devicesH1S.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em2));
  devicesH1S.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em2));

  NS_LOG_INFO("已配置信道错误模型，错误率为: " << errorRate * 100 << "%");

  // 安装Internet协议栈
  InternetStackHelper internet;
  internet.Install(hosts);
  internet.Install(switchNode);

  // 分配IP地址
  Ipv4AddressHelper address;
  
  // Host0-Switch网段
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesH0S = address.Assign(devicesH0S);
  
  // Host1-Switch网段
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesH1S = address.Assign(devicesH1S);

  // 启用全局路由
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // 获取各节点IP地址
  Ipv4Address host0Addr = interfacesH0S.GetAddress(0);
  Ipv4Address host1Addr = interfacesH1S.GetAddress(0);
  Ipv4Address switchAddr0 = interfacesH0S.GetAddress(1);
  Ipv4Address switchAddr1 = interfacesH1S.GetAddress(1);

  NS_LOG_INFO("主机0 IP地址: " << host0Addr);
  NS_LOG_INFO("主机1 IP地址: " << host1Addr);
  NS_LOG_INFO("交换机接口0 IP地址: " << switchAddr0);
  NS_LOG_INFO("交换机接口1 IP地址: " << switchAddr1);

  // 创建交换机应用
  Ptr<IncSwitch> incSwitch = CreateObject<IncSwitch>();
  incSwitch->SetSwitchId("Switch0");
  incSwitch->SetStartTime(Seconds(0.5));
  incSwitch->SetStopTime(Seconds(10000.0));
  switchNode.Get(0)->AddApplication(incSwitch);

  // 设置不同的QP号，从1开始编号
  uint16_t host0QP = 1;       // 主机0的QP号
  uint16_t host1QP = 2;       // 主机1的QP号
  uint16_t switchQP0 = 3;     // 交换机连接主机0的QP号
  uint16_t switchQP1 = 4;     // 交换机连接主机1的QP号
  
  uint16_t groupId = 100;    // 通信组ID
  uint16_t fanIn = 2;        // 扇入度为2（两个主机）
  uint16_t arraySize = 2048;   // 数组大小为2048

  // 初始化交换机配置
  // 准备链接状态信息（srcIP, srcQP, dstIP, dstQP, is_father）
  // 注意: src和dst指的是出站的方向，即当前节点为src，目的节点为dst
  // is_father=true表示到子节点的链路，false表示到父节点的链路
  std::vector<std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, bool>> linkState;
  
  // 交换机 -> 主机0 (父节点到子节点)
  linkState.push_back(std::make_tuple(switchAddr0, switchQP0, host0Addr, host0QP, true));
  
  // 交换机 -> 主机1 (父节点到子节点)
  linkState.push_back(std::make_tuple(switchAddr1, switchQP1, host1Addr, host1QP, true));
  
  // 初始化交换机引擎 - 会自动初始化所有查询表
  incSwitch->InitializeEngine(linkState, groupId, fanIn, arraySize);

  // 为主机0上的IncStack配置
  Ptr<IncStack> incStack0 = CreateObject<IncStack>();
  incStack0->SetServerId("Host0");
  incStack0->SetStartTime(Seconds(1.0));
  incStack0->SetStopTime(Seconds(10000.0));
  hosts.Get(0)->AddApplication(incStack0);
  
  // 配置IncStack0参数
  incStack0->SetGroupId(groupId);
  incStack0->SetOperation(IncHeader::SUM);
  incStack0->SetDataType(IncHeader::INT32);
  incStack0->SetFillValue(1);  // 填充值为1
  incStack0->SetWindowSize(2048); // 滑动窗口大小为2048
  incStack0->SetLocal(host0Addr, host0QP); // 使用主机0的QP
  incStack0->SetRemote(switchAddr0, switchQP0); // 连接到交换机的相应QP
  incStack0->SetTotalPackets(3);
  incStack0->SetCompleteCallback(MakeBoundCallback(&AllReduceCompletionCallback, std::string("Host0")));
  
  // 为主机1上的IncStack配置
  Ptr<IncStack> incStack1 = CreateObject<IncStack>();
  incStack1->SetServerId("Host1");
  incStack1->SetStartTime(Seconds(1.0));
  incStack1->SetStopTime(Seconds(10000.0));
  hosts.Get(1)->AddApplication(incStack1);
  
  // 配置IncStack1参数
  incStack1->SetGroupId(groupId);
  incStack1->SetOperation(IncHeader::SUM);
  incStack1->SetDataType(IncHeader::INT32);
  incStack1->SetFillValue(1);  // 填充值为1
  incStack1->SetWindowSize(2048); // 滑动窗口大小为2048
  incStack1->SetLocal(host1Addr, host1QP); // 使用主机1的QP
  incStack1->SetRemote(switchAddr1, switchQP1); // 连接到交换机的相应QP
  incStack1->SetTotalPackets(3);
  incStack1->SetCompleteCallback(MakeBoundCallback(&AllReduceCompletionCallback, std::string("Host1")));
  
  // 在2秒时启动Host0的AllReduce
  Simulator::Schedule(Seconds(2.0), &IncStack::AllReduce, incStack0);
  
  // 在2秒时启动Host1的AllReduce
  Simulator::Schedule(Seconds(2.0), &IncStack::AllReduce, incStack1);
  
  // 运行仿真
  NS_LOG_INFO("开始运行仿真...");
  Simulator::Run();
  Simulator::Destroy();
  NS_LOG_INFO("仿真结束");

  return 0;
} 