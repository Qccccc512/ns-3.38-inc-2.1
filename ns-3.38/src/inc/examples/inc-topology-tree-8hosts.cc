/*
 * 在网计算协议 - 模拟测试：树状拓扑，7个交换机连接8个主机
 * 拓扑结构:
 *                  Switch 1 (根节点)
 *                /            \
 *         Switch 2            Switch 3
 *        /       \           /       \
 *   Switch 4   Switch 5   Switch 6   Switch 7
 *     /  \      /  \       /  \      /  \
 *    H1   H2   H3   H4    H5   H6   H7   H8
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

NS_LOG_COMPONENT_DEFINE("IncTreeTopology8Hosts");

// 回调函数，用于在AllReduce完成时通知
void AllReduceCompletionCallback(std::string id)
{
  NS_LOG_UNCOND("时间 " << Simulator::Now().GetSeconds() << "s: 主机 " << id << " 完成 AllReduce 操作");
}

int
main(int argc, char* argv[])
{
  // 命令行参数处理
  double errorRate = 0.0;           // 默认错误率0%
  uint32_t dataSize = 1024 * 2;           // 默认数据大小：64个数据包
  std::string dataRate = "1Gbps";   // 默认带宽：1Gbps
  std::string delay = "1ms";        // 默认时延：1ms
  uint32_t windowSize = 2048;         // 默认窗口大小：32
  uint32_t arraySize = 2048;        // 默认数组大小：1024
  
  CommandLine cmd(__FILE__);
  cmd.AddValue("error", "链路错误率", errorRate);
  cmd.AddValue("size", "发送数据包数量", dataSize);
  cmd.AddValue("datarate", "链路带宽", dataRate);
  cmd.AddValue("delay", "链路时延", delay);
  cmd.AddValue("window", "滑动窗口大小", windowSize);
  cmd.AddValue("array", "交换机数组大小", arraySize);
  cmd.Parse(argc, argv);

  // 日志组件配置
  LogComponentEnable("IncTreeTopology8Hosts", LOG_LEVEL_INFO);
  LogComponentEnable("IncStack", LOG_LEVEL_WARN);
  LogComponentEnable("IncSwitch", LOG_LEVEL_WARN);

  NS_LOG_INFO("已配置链路错误模型，错误率为: " << errorRate * 100 << "%");
  NS_LOG_INFO("链路带宽: " << dataRate << ", 时延: " << delay);
  NS_LOG_INFO("数据包数量: " << dataSize << ", 窗口大小: " << windowSize << ", 数组大小: " << arraySize);

  // 创建15个节点：7个交换机和8个主机
  NodeContainer switchNodes;
  switchNodes.Create(7);  // Switch 1-7
  
  NodeContainer hostNodes;
  hostNodes.Create(8);    // Host 1-8

  // 创建点对点链路
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
  p2p.SetChannelAttribute("Delay", StringValue(delay));

  // 存储所有的网络设备
  NetDeviceContainer devices[14]; // 7个交换机+8个主机之间的链路共14条
  
  // 交换机之间的链路
  // 1<->2
  devices[0] = p2p.Install(switchNodes.Get(0), switchNodes.Get(1));
  
  // 1<->3
  devices[1] = p2p.Install(switchNodes.Get(0), switchNodes.Get(2));
  
  // 2<->4
  devices[2] = p2p.Install(switchNodes.Get(1), switchNodes.Get(3));
  
  // 2<->5
  devices[3] = p2p.Install(switchNodes.Get(1), switchNodes.Get(4));
  
  // 3<->6
  devices[4] = p2p.Install(switchNodes.Get(2), switchNodes.Get(5));
  
  // 3<->7
  devices[5] = p2p.Install(switchNodes.Get(2), switchNodes.Get(6));
  
  // 交换机到主机的链路
  // 4<->Host1
  devices[6] = p2p.Install(switchNodes.Get(3), hostNodes.Get(0));
  
  // 4<->Host2
  devices[7] = p2p.Install(switchNodes.Get(3), hostNodes.Get(1));
  
  // 5<->Host3
  devices[8] = p2p.Install(switchNodes.Get(4), hostNodes.Get(2));
  
  // 5<->Host4
  devices[9] = p2p.Install(switchNodes.Get(4), hostNodes.Get(3));
  
  // 6<->Host5
  devices[10] = p2p.Install(switchNodes.Get(5), hostNodes.Get(4));
  
  // 6<->Host6
  devices[11] = p2p.Install(switchNodes.Get(5), hostNodes.Get(5));
  
  // 7<->Host7
  devices[12] = p2p.Install(switchNodes.Get(6), hostNodes.Get(6));
  
  // 7<->Host8
  devices[13] = p2p.Install(switchNodes.Get(6), hostNodes.Get(7));
  
  // 为每条链路添加错误模型
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(errorRate));
  em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
  
  for (int i = 0; i < 14; i++) {
    devices[i].Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    devices[i].Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  }

  // 安装互联网协议栈
  InternetStackHelper internet;
  internet.Install(switchNodes);
  internet.Install(hostNodes);

  // 分配IP地址
  Ipv4AddressHelper ipv4;
  Ipv4InterfaceContainer interfaces[14];
  
  for (int i = 0; i < 14; i++) {
    std::ostringstream subnet;
    subnet << "10.1." << (i+1) << ".0";
    ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
    interfaces[i] = ipv4.Assign(devices[i]);
  }
  
  // 设置IP路由
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // 定义全局唯一的QP号，从1开始编号
  // 交换机和主机的QP号
  uint16_t qpCounter = 1;
  
  // 存储交换机的QP
  struct SwitchQPs {
    uint16_t toParent;  // 到父节点的QP
    std::vector<uint16_t> toChildren;  // 到子节点的QP列表
  };
  
  SwitchQPs switchQPs[7];
  
  // 根节点交换机(1)没有父节点
  switchQPs[0].toParent = 0;  // 不使用
  switchQPs[0].toChildren.push_back(qpCounter++);  // 到交换机2的QP
  switchQPs[0].toChildren.push_back(qpCounter++);  // 到交换机3的QP
  
  // 交换机2
  switchQPs[1].toParent = qpCounter++;  // 到交换机1的QP
  switchQPs[1].toChildren.push_back(qpCounter++);  // 到交换机4的QP
  switchQPs[1].toChildren.push_back(qpCounter++);  // 到交换机5的QP
  
  // 交换机3
  switchQPs[2].toParent = qpCounter++;  // 到交换机1的QP
  switchQPs[2].toChildren.push_back(qpCounter++);  // 到交换机6的QP
  switchQPs[2].toChildren.push_back(qpCounter++);  // 到交换机7的QP
  
  // 交换机4
  switchQPs[3].toParent = qpCounter++;  // 到交换机2的QP
  switchQPs[3].toChildren.push_back(qpCounter++);  // 到主机1的QP
  switchQPs[3].toChildren.push_back(qpCounter++);  // 到主机2的QP
  
  // 交换机5
  switchQPs[4].toParent = qpCounter++;  // 到交换机2的QP
  switchQPs[4].toChildren.push_back(qpCounter++);  // 到主机3的QP
  switchQPs[4].toChildren.push_back(qpCounter++);  // 到主机4的QP
  
  // 交换机6
  switchQPs[5].toParent = qpCounter++;  // 到交换机3的QP
  switchQPs[5].toChildren.push_back(qpCounter++);  // 到主机5的QP
  switchQPs[5].toChildren.push_back(qpCounter++);  // 到主机6的QP
  
  // 交换机7
  switchQPs[6].toParent = qpCounter++;  // 到交换机3的QP
  switchQPs[6].toChildren.push_back(qpCounter++);  // 到主机7的QP
  switchQPs[6].toChildren.push_back(qpCounter++);  // 到主机8的QP
  
  // 主机的QP号
  uint16_t hostQPs[8];
  for (int i = 0; i < 8; i++) {
    hostQPs[i] = qpCounter++;
  }
  
  // 创建和配置7个交换机
  Ptr<IncSwitch> switches[7];
  
  for (int i = 0; i < 7; i++) {
    switches[i] = CreateObject<IncSwitch>();
    switchNodes.Get(i)->AddApplication(switches[i]);
    switches[i]->SetStartTime(Seconds(0.5));
    switches[i]->SetStopTime(Seconds(10000.0));
    
    std::ostringstream switchId;
    switchId << "Switch" << (i+1);
    switches[i]->SetSwitchId(switchId.str());
    
    // 为每个交换机准备链路状态信息
    std::vector<std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, bool>> linkState;
    
    // 添加到父节点的链路（除了根节点）
    if (i > 0) {
      int parentIdx = (i - 1) / 2;  // 父节点的索引
      int childPosition = (i % 2 == 1) ? 0 : 1;  // 左子节点为0，右子节点为1
      
      // 确定接口的索引
      int interfaceIdx = 0;
      if (parentIdx == 0) {
        interfaceIdx = childPosition;  // 交换机1到交换机2或3的链路
      } else if (parentIdx == 1) {
        interfaceIdx = childPosition + 2;  // 交换机2到交换机4或5的链路
      } else if (parentIdx == 2) {
        interfaceIdx = childPosition + 4;  // 交换机3到交换机6或7的链路
      }
      
      linkState.push_back(std::make_tuple(
        interfaces[interfaceIdx].GetAddress(1),  // 当前交换机的IP
        switchQPs[i].toParent,                  // 当前交换机到父节点的QP
        interfaces[interfaceIdx].GetAddress(0),  // 父交换机的IP
        switchQPs[parentIdx].toChildren[childPosition], // 父交换机到当前交换机的QP
        false                                   // to_father_or_son=false
      ));
    }
    
    // 添加到子节点的链路
    for (size_t j = 0; j < switchQPs[i].toChildren.size(); j++) {
      int childIdx = 2 * i + j + 1;  // 子节点的索引
      
      // 确定接口的索引
      int interfaceIdx = 0;
      if (i == 0) {
        interfaceIdx = j;  // 交换机1到交换机2或3的链路
      } else if (i == 1) {
        interfaceIdx = j + 2;  // 交换机2到交换机4或5的链路
      } else if (i == 2) {
        interfaceIdx = j + 4;  // 交换机3到交换机6或7的链路
      } else if (i == 3) {
        interfaceIdx = j + 6;  // 交换机4到主机1或2的链路
      } else if (i == 4) {
        interfaceIdx = j + 8;  // 交换机5到主机3或4的链路
      } else if (i == 5) {
        interfaceIdx = j + 10;  // 交换机6到主机5或6的链路
      } else if (i == 6) {
        interfaceIdx = j + 12;  // 交换机7到主机7或8的链路
      }
      
      // 如果子节点是交换机
      if (childIdx < 7) {
        linkState.push_back(std::make_tuple(
          interfaces[interfaceIdx].GetAddress(0),  // 当前交换机的IP
          switchQPs[i].toChildren[j],              // 当前交换机到子节点的QP
          interfaces[interfaceIdx].GetAddress(1),  // 子交换机的IP
          switchQPs[childIdx].toParent,           // 子交换机到当前交换机的QP
          true                                    // to_father_or_son=true
        ));
      } else {
        // 子节点是主机
        int hostIdx = childIdx - 7;  // 主机的索引
        linkState.push_back(std::make_tuple(
          interfaces[interfaceIdx].GetAddress(0),  // 当前交换机的IP
          switchQPs[i].toChildren[j],              // 当前交换机到主机的QP
          interfaces[interfaceIdx].GetAddress(1),  // 主机的IP
          hostQPs[hostIdx],                       // 主机的QP
          true                                    // to_father_or_son=true
        ));
      }
    }
    
    // 设置组参数
    uint16_t groupId = 1;     // 通信组ID
    uint16_t fanIn = 2;       // 扇入度，每个交换机连接2个子节点
    
    // 初始化交换机引擎
    switches[i]->InitializeEngine(linkState, groupId, fanIn, arraySize);
  }
  
  // 创建并配置8个主机上的INC协议栈
  Ptr<IncStack> incStacks[8];
  
  for (int i = 0; i < 8; i++) {
    incStacks[i] = CreateObject<IncStack>();
    hostNodes.Get(i)->AddApplication(incStacks[i]);
    incStacks[i]->SetStartTime(Seconds(1.0));
    incStacks[i]->SetStopTime(Seconds(10000.0));
    
    std::ostringstream hostId;
    hostId << "Host" << (i+1);
    incStacks[i]->SetServerId(hostId.str());
    
    // 确定连接到主机的交换机编号
    int switchIdx = i / 2 + 3;  // 主机1-2连接到交换机4，主机3-4连接到交换机5，以此类推
    int hostPosition = i % 2;   // 0或1，表示是交换机的左或右子节点
    
    // 确定接口的索引
    int interfaceIdx = i + 6;  // 主机1对应interfaceIdx=6，依此类推
    
    incStacks[i]->SetRemote(interfaces[interfaceIdx].GetAddress(0), switchQPs[switchIdx].toChildren[hostPosition]);
    incStacks[i]->SetLocal(interfaces[interfaceIdx].GetAddress(1), hostQPs[i]);
    incStacks[i]->SetCompleteCallback(MakeBoundCallback(&AllReduceCompletionCallback, hostId.str()));
    incStacks[i]->SetWindowSize(windowSize);
    incStacks[i]->SetOperation(IncHeader::SUM);
    incStacks[i]->SetDataType(IncHeader::INT32);
    incStacks[i]->SetTotalPackets(dataSize);
    incStacks[i]->SetFillValue(1); // 所有主机的测试数据值设为1
    incStacks[i]->SetGroupId(1);   // 组ID设为1
    
    // 打印主机的IP地址和连接信息
    NS_LOG_INFO("主机" << (i+1) << " IP地址: " << interfaces[interfaceIdx].GetAddress(1));
  }
  
  // 打印交换机接口的IP地址
  for (int i = 0; i < 7; i++) {
    for (uint32_t j = 0; j < switchNodes.Get(i)->GetNDevices(); j++) {
      Ptr<NetDevice> device = switchNodes.Get(i)->GetDevice(j);
      Ptr<Ipv4> ipv4 = device->GetNode()->GetObject<Ipv4>();
      for (uint32_t k = 0; k < ipv4->GetNInterfaces(); k++) {
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(k, 0);
        NS_LOG_INFO("交换机" << (i+1) << " 接口" << k << " IP地址: " << iaddr.GetLocal());
      }
    }
  }
  
  NS_LOG_INFO("启动配置完成，将在2秒后开始AllReduce操作");
  
  // 同时启动各个主机的AllReduce操作
  for (int i = 0; i < 8; i++) {
    Simulator::Schedule(Seconds(2.0), &IncStack::AllReduce, incStacks[i]);
  }
  
  // 启动模拟
  NS_LOG_INFO("开始运行仿真...");
  
  // 设置Pcap跟踪
  p2p.EnablePcapAll("inc-topology-tree-8hosts", false);
  
  Simulator::Run();
  Simulator::Destroy();
  
  NS_LOG_INFO("仿真结束");
  
  return 0;
} 