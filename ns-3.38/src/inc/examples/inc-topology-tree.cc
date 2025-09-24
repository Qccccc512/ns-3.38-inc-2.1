/*
 * 在网计算协议 - 模拟测试：树状拓扑，3个交换机连接4个主机
 * 拓扑结构:
 *            Switch A (根节点)
 *           /          \
 *     Switch B        Switch C
 *     /      \        /      \
 *   Host1   Host2   Host3   Host4
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

NS_LOG_COMPONENT_DEFINE("IncTreeTopology");

// 回调函数，用于在AllReduce完成时通知
void AllReduceCompletionCallback(std::string id)
{
  NS_LOG_UNCOND("时间 " << Simulator::Now().GetSeconds() << "s: 主机 " << id << " 完成 AllReduce 操作");
}

int
main(int argc, char* argv[])
{
  // 命令行参数处理
  double errorRate = 0.0;  // 默认错误率1%
  uint32_t dataSize = 1024 * 2;    // 默认数据大小：3个数据包，减少数据大小以便调试
  CommandLine cmd(__FILE__);
  cmd.AddValue("error", "Error rate for channels", errorRate);
  cmd.AddValue("size", "Number of data packets to send", dataSize);
  cmd.Parse(argc, argv);

  // 日志组件配置
  LogComponentEnable("IncTreeTopology", LOG_LEVEL_INFO);
  LogComponentEnable("IncStack", LOG_LEVEL_WARN);
  LogComponentEnable("IncSwitch", LOG_LEVEL_WARN);

  // 创建8个节点：3个交换机和4个主机，另外还有1个节点用于运行模拟器
  NodeContainer switchNodes;
  switchNodes.Create(3);  // Switch A, B, C
  
  NodeContainer hostNodes;
  hostNodes.Create(4);    // Host 1, 2, 3, 4

  // 创建点对点链路
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));

  // 创建链路并安装到各节点之间
  // 交换机A <-> 交换机B
  NetDeviceContainer devAB = p2p.Install(switchNodes.Get(0), switchNodes.Get(1));
  
  // 交换机A <-> 交换机C
  NetDeviceContainer devAC = p2p.Install(switchNodes.Get(0), switchNodes.Get(2));
  
  // 交换机B <-> 主机1
  NetDeviceContainer devB1 = p2p.Install(switchNodes.Get(1), hostNodes.Get(0));
  
  // 交换机B <-> 主机2
  NetDeviceContainer devB2 = p2p.Install(switchNodes.Get(1), hostNodes.Get(1));
  
  // 交换机C <-> 主机3
  NetDeviceContainer devC3 = p2p.Install(switchNodes.Get(2), hostNodes.Get(2));
  
  // 交换机C <-> 主机4
  NetDeviceContainer devC4 = p2p.Install(switchNodes.Get(2), hostNodes.Get(3));
  
  // 为每条链路添加错误模型
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(errorRate));
  em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
  
  devAB.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  devAB.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  
  devAC.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  devAC.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  
  devB1.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  devB1.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  
  devB2.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  devB2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  
  devC3.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  devC3.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  
  devC4.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  devC4.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

  // 安装互联网协议栈
  InternetStackHelper internet;
  internet.Install(switchNodes);
  internet.Install(hostNodes);

  // 分配IP地址
  Ipv4AddressHelper ipv4;
  
  // 交换机A <-> 交换机B
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = ipv4.Assign(devAB);
  
  // 交换机A <-> 交换机C
  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAC = ipv4.Assign(devAC);
  
  // 交换机B <-> 主机1
  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesB1 = ipv4.Assign(devB1);
  
  // 交换机B <-> 主机2
  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesB2 = ipv4.Assign(devB2);
  
  // 交换机C <-> 主机3
  ipv4.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesC3 = ipv4.Assign(devC3);
  
  // 交换机C <-> 主机4
  ipv4.SetBase("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesC4 = ipv4.Assign(devC4);
  
  // 设置IP路由
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // 定义全局唯一的QP号，从1开始编号
  // 交换机A的QP
  uint16_t switchAQP_toB = 1;       // 交换机A到交换机B的QP
  uint16_t switchAQP_toC = 2;       // 交换机A到交换机C的QP
  
  // 交换机B的QP
  uint16_t switchBQP_toA = 3;       // 交换机B到交换机A的QP
  uint16_t switchBQP_to1 = 4;       // 交换机B到主机1的QP
  uint16_t switchBQP_to2 = 5;       // 交换机B到主机2的QP
  
  // 交换机C的QP
  uint16_t switchCQP_toA = 6;       // 交换机C到交换机A的QP
  uint16_t switchCQP_to3 = 7;       // 交换机C到主机3的QP
  uint16_t switchCQP_to4 = 8;       // 交换机C到主机4的QP
  
  // 主机的QP
  uint16_t host1QP = 9;             // 主机1到交换机B的QP
  uint16_t host2QP = 10;            // 主机2到交换机B的QP
  uint16_t host3QP = 11;            // 主机3到交换机C的QP
  uint16_t host4QP = 12;            // 主机4到交换机C的QP

  // 创建并配置交换机A
  Ptr<IncSwitch> switchA = CreateObject<IncSwitch>();
  switchNodes.Get(0)->AddApplication(switchA);
  switchA->SetStartTime(Seconds(0.5));
  switchA->SetStopTime(Seconds(10000.0));
  switchA->SetSwitchId("SwitchA");
  
  // 交换机A作为根节点，连接两个子节点B和C
  std::vector<std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, bool>> linkStateA;
  
  // 交换机A到交换机B的链路（to_son=true表示到子节点的链路）
  linkStateA.push_back(std::make_tuple(
    interfacesAB.GetAddress(0),     // A的IP
    switchAQP_toB,                  // A的QP
    interfacesAB.GetAddress(1),     // B的IP
    switchBQP_toA,                  // B的QP
    true                           // to_son=true
  ));
  
  // 交换机A到交换机C的链路（to_son=true表示到子节点的链路）
  linkStateA.push_back(std::make_tuple(
    interfacesAC.GetAddress(0),     // A的IP
    switchAQP_toC,                  // A的QP
    interfacesAC.GetAddress(1),     // C的IP
    switchCQP_toA,                  // C的QP
    true                           // to_son=true
  ));
  
  // 创建并配置交换机B
  Ptr<IncSwitch> switchB = CreateObject<IncSwitch>();
  switchNodes.Get(1)->AddApplication(switchB);
  switchB->SetStartTime(Seconds(0.5));
  switchB->SetStopTime(Seconds(10000.0));
  switchB->SetSwitchId("SwitchB");
  
  // 交换机B连接一个父节点A和两个子节点Host1和Host2
  std::vector<std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, bool>> linkStateB;
  
  // 交换机B到交换机A的链路（to_father_or_son=false表示到父节点的链路）
  linkStateB.push_back(std::make_tuple(
    interfacesAB.GetAddress(1),     // B的IP
    switchBQP_toA,                  // B的QP
    interfacesAB.GetAddress(0),     // A的IP
    switchAQP_toB,                  // A的QP
    false                          // to_father_or_son=false
  ));
  
  // 交换机B到主机1的链路（to_father_or_son=true表示到子节点的链路）
  linkStateB.push_back(std::make_tuple(
    interfacesB1.GetAddress(0),     // B的IP
    switchBQP_to1,                  // B的QP
    interfacesB1.GetAddress(1),     // Host1的IP
    host1QP,                        // Host1的QP
    true                           // to_father_or_son=true
  ));
  
  // 交换机B到主机2的链路（to_father_or_son=true表示到子节点的链路）
  linkStateB.push_back(std::make_tuple(
    interfacesB2.GetAddress(0),     // B的IP
    switchBQP_to2,                  // B的QP
    interfacesB2.GetAddress(1),     // Host2的IP
    host2QP,                        // Host2的QP
    true                           // to_father_or_son=true
  ));
  
  // 创建并配置交换机C
  Ptr<IncSwitch> switchC = CreateObject<IncSwitch>();
  switchNodes.Get(2)->AddApplication(switchC);
  switchC->SetStartTime(Seconds(0.5));
  switchC->SetStopTime(Seconds(10000.0));
  switchC->SetSwitchId("SwitchC");
  
  // 交换机C连接一个父节点A和两个子节点Host3和Host4
  std::vector<std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, bool>> linkStateC;
  
  // 交换机C到交换机A的链路（to_father_or_son=false表示到父节点的链路）
  linkStateC.push_back(std::make_tuple(
    interfacesAC.GetAddress(1),     // C的IP
    switchCQP_toA,                  // C的QP
    interfacesAC.GetAddress(0),     // A的IP
    switchAQP_toC,                  // A的QP
    false                          // to_father_or_son=false
  ));
  
  // 交换机C到主机3的链路（to_father_or_son=true表示到子节点的链路）
  linkStateC.push_back(std::make_tuple(
    interfacesC3.GetAddress(0),     // C的IP
    switchCQP_to3,                  // C的QP
    interfacesC3.GetAddress(1),     // Host3的IP
    host3QP,                        // Host3的QP
    true                           // to_father_or_son=true
  ));
  
  // 交换机C到主机4的链路（to_father_or_son=true表示到子节点的链路）
  linkStateC.push_back(std::make_tuple(
    interfacesC4.GetAddress(0),     // C的IP
    switchCQP_to4,                  // C的QP
    interfacesC4.GetAddress(1),     // Host4的IP
    host4QP,                        // Host4的QP
    true                           // to_father_or_son=true
  ));
  
  // 设置组参数
  uint16_t groupId = 1;     // 通信组ID
  uint16_t fanIn = 2;       // 扇入度，每个交换机连接2个子节点
  uint16_t arraySize = 2048;  // 缓冲区和数组大小，修改为更合理的值
  
  // 初始化交换机
  switchA->InitializeEngine(linkStateA, groupId, fanIn, arraySize);
  switchB->InitializeEngine(linkStateB, groupId, fanIn, arraySize);
  switchC->InitializeEngine(linkStateC, groupId, fanIn, arraySize);
  
  // 打印各个网络接口的IP地址，用于调试
  NS_LOG_INFO("交换机A到B连接: " << interfacesAB.GetAddress(0) << " <-> " << interfacesAB.GetAddress(1));
  NS_LOG_INFO("交换机A到C连接: " << interfacesAC.GetAddress(0) << " <-> " << interfacesAC.GetAddress(1));
  NS_LOG_INFO("交换机B到Host1连接: " << interfacesB1.GetAddress(0) << " <-> " << interfacesB1.GetAddress(1));
  NS_LOG_INFO("交换机B到Host2连接: " << interfacesB2.GetAddress(0) << " <-> " << interfacesB2.GetAddress(1));
  NS_LOG_INFO("交换机C到Host3连接: " << interfacesC3.GetAddress(0) << " <-> " << interfacesC3.GetAddress(1));
  NS_LOG_INFO("交换机C到Host4连接: " << interfacesC4.GetAddress(0) << " <-> " << interfacesC4.GetAddress(1));
  
  // 创建并配置主机1上的INC协议栈
  Ptr<IncStack> incStack1 = CreateObject<IncStack>();
  hostNodes.Get(0)->AddApplication(incStack1);
  incStack1->SetStartTime(Seconds(1.0));
  incStack1->SetStopTime(Seconds(10000.0));
  incStack1->SetServerId("Host1");
  incStack1->SetRemote(interfacesB1.GetAddress(0), switchBQP_to1);
  incStack1->SetLocal(interfacesB1.GetAddress(1), host1QP);
  incStack1->SetCompleteCallback(MakeBoundCallback(&AllReduceCompletionCallback, std::string("Host1")));
  incStack1->SetWindowSize(2048);  // 降低滑动窗口大小以减少重传压力
  incStack1->SetOperation(IncHeader::SUM);
  incStack1->SetDataType(IncHeader::INT32);
  incStack1->SetTotalPackets(dataSize);
  incStack1->SetFillValue(1); // Host1的测试数据值为1
  
  // 创建并配置主机2上的INC协议栈
  Ptr<IncStack> incStack2 = CreateObject<IncStack>();
  hostNodes.Get(1)->AddApplication(incStack2);
  incStack2->SetStartTime(Seconds(1.0));
  incStack2->SetStopTime(Seconds(10000.0));
  incStack2->SetServerId("Host2");
  incStack2->SetRemote(interfacesB2.GetAddress(0), switchBQP_to2);
  incStack2->SetLocal(interfacesB2.GetAddress(1), host2QP);
  incStack2->SetCompleteCallback(MakeBoundCallback(&AllReduceCompletionCallback, std::string("Host2")));
  incStack2->SetWindowSize(2048);  // 降低滑动窗口大小以减少重传压力
  incStack2->SetOperation(IncHeader::SUM);
  incStack2->SetDataType(IncHeader::INT32);
  incStack2->SetTotalPackets(dataSize);
  incStack2->SetFillValue(1); // Host2的测试数据值为2
  
  // 创建并配置主机3上的INC协议栈
  Ptr<IncStack> incStack3 = CreateObject<IncStack>();
  hostNodes.Get(2)->AddApplication(incStack3);
  incStack3->SetStartTime(Seconds(1.0));
  incStack3->SetStopTime(Seconds(10000.0));
  incStack3->SetServerId("Host3");
  incStack3->SetRemote(interfacesC3.GetAddress(0), switchCQP_to3);
  incStack3->SetLocal(interfacesC3.GetAddress(1), host3QP);
  incStack3->SetCompleteCallback(MakeBoundCallback(&AllReduceCompletionCallback, std::string("Host3")));
  incStack3->SetWindowSize(2048);  // 降低滑动窗口大小以减少重传压力
  incStack3->SetOperation(IncHeader::SUM);
  incStack3->SetDataType(IncHeader::INT32);
  incStack3->SetTotalPackets(dataSize);
  incStack3->SetFillValue(1); // Host3的测试数据值为3
  
  // 创建并配置主机4上的INC协议栈
  Ptr<IncStack> incStack4 = CreateObject<IncStack>();
  hostNodes.Get(3)->AddApplication(incStack4);
  incStack4->SetStartTime(Seconds(1.0));
  incStack4->SetStopTime(Seconds(10000.0));
  incStack4->SetServerId("Host4");
  incStack4->SetRemote(interfacesC4.GetAddress(0), switchCQP_to4);
  incStack4->SetLocal(interfacesC4.GetAddress(1), host4QP);
  incStack4->SetCompleteCallback(MakeBoundCallback(&AllReduceCompletionCallback, std::string("Host4")));
  incStack4->SetWindowSize(2048);  // 降低滑动窗口大小以减少重传压力
  incStack4->SetOperation(IncHeader::SUM);
  incStack4->SetDataType(IncHeader::INT32);
  incStack4->SetTotalPackets(dataSize);
  incStack4->SetFillValue(1); // Host4的测试数据值为4
  
  // 添加组ID配置
  incStack1->SetGroupId(groupId);
  incStack2->SetGroupId(groupId);
  incStack3->SetGroupId(groupId);
  incStack4->SetGroupId(groupId);
  
  NS_LOG_INFO("启动配置完成，将在2秒后开始AllReduce操作");
  
  // 同时启动各个主机的AllReduce操作
  Simulator::Schedule(Seconds(2.0), &IncStack::AllReduce, incStack1);
  Simulator::Schedule(Seconds(2.0), &IncStack::AllReduce, incStack2);
  Simulator::Schedule(Seconds(2.0), &IncStack::AllReduce, incStack3);
  Simulator::Schedule(Seconds(2.0), &IncStack::AllReduce, incStack4);
  
  // 启动模拟
  NS_LOG_INFO("开始在网计算协议树状拓扑模拟");
  
  // 设置Pcap跟踪
  //p2p.EnablePcapAll("inc-topology-tree", false);
  
  Simulator::Run();
  Simulator::Destroy();
  
  NS_LOG_INFO("模拟结束");
  
  return 0;
} 