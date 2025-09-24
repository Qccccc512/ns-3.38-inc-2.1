/*
 * 在网计算协议 - 模拟测试：树状拓扑，31个交换机连接32个主机
 * 拓扑结构:
 *                                  Switch 1 (根节点)
 *                                /                 \
 *                       Switch 2                   Switch 3
 *                     /         \                 /         \
 *               Switch 4         Switch 5   Switch 6         Switch 7
 *              /       \        /       \  /       \        /       \
 *        Sw8     Sw9    Sw10    Sw11   Sw12    Sw13     Sw14     Sw15
 *       /  \    /  \    /  \    /  \   /  \    /  \     /  \     /  \
 *     S16 S17 S18 S19 S20 S21 S22 S23 S24 S25 S26 S27 S28 S29 S30 S31
 *     /\  /\  /\  /\  /\  /\  /\  /\  /\  /\  /\  /\  /\  /\  /\  /\
 *    H1 H2 ... ... ... ... ... ... ... ... ... ... ... ... ... H31 H32
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

NS_LOG_COMPONENT_DEFINE("IncTreeTopology32Hosts");

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
  LogComponentEnable("IncTreeTopology32Hosts", LOG_LEVEL_INFO);
  LogComponentEnable("IncStack", LOG_LEVEL_WARN);
  LogComponentEnable("IncSwitch", LOG_LEVEL_WARN);

  NS_LOG_INFO("已配置链路错误模型，错误率为: " << errorRate * 100 << "%");
  NS_LOG_INFO("链路带宽: " << dataRate << ", 时延: " << delay);
  NS_LOG_INFO("数据包数量: " << dataSize << ", 窗口大小: " << windowSize << ", 数组大小: " << arraySize);

  // 创建63个节点：31个交换机和32个主机
  NodeContainer switchNodes;
  switchNodes.Create(31);  // Switch 1-31
  
  NodeContainer hostNodes;
  hostNodes.Create(32);    // Host 1-32

  // 创建点对点链路
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
  p2p.SetChannelAttribute("Delay", StringValue(delay));

  // 存储所有的网络设备
  NetDeviceContainer devices[62]; // 31个交换机+32个主机之间的链路共62条
  
  // 创建链路
  int deviceIdx = 0;
  
  // 使用循环创建所有交换机之间的链路 (共30条)
  for (int i = 0; i < 15; i++) {
    // 每个非叶子交换机连接两个子交换机
    devices[deviceIdx++] = p2p.Install(switchNodes.Get(i), switchNodes.Get(2*i+1));
    devices[deviceIdx++] = p2p.Install(switchNodes.Get(i), switchNodes.Get(2*i+2));
  }
  
  // 叶子交换机到主机的链路 (共32条)
  for (int i = 15; i < 31; i++) {
    // 每个叶子交换机连接两个主机
    devices[deviceIdx++] = p2p.Install(switchNodes.Get(i), hostNodes.Get(2*(i-15)));
    devices[deviceIdx++] = p2p.Install(switchNodes.Get(i), hostNodes.Get(2*(i-15)+1));
  }
  
  // 为每条链路添加错误模型
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(errorRate));
  em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
  
  for (int i = 0; i < 62; i++) {
    devices[i].Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    devices[i].Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  }

  // 安装互联网协议栈
  InternetStackHelper internet;
  internet.Install(switchNodes);
  internet.Install(hostNodes);

  // 分配IP地址
  Ipv4AddressHelper ipv4;
  Ipv4InterfaceContainer interfaces[62];
  
  for (int i = 0; i < 62; i++) {
    std::ostringstream subnet;
    subnet << "10.1." << (i+1) << ".0";
    ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
    interfaces[i] = ipv4.Assign(devices[i]);
  }
  
  // 设置IP路由
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // 定义全局唯一的QP号，从1开始编号
  uint16_t qpCounter = 1;
  
  // 存储交换机的QP
  struct SwitchQPs {
    uint16_t toParent;  // 到父节点的QP
    std::vector<uint16_t> toChildren;  // 到子节点的QP列表
  };
  
  SwitchQPs switchQPs[31];
  
  // 为所有交换机分配QP号
  // 根节点交换机(1)没有父节点
  switchQPs[0].toParent = 0;  // 不使用
  switchQPs[0].toChildren.push_back(qpCounter++);  // 到左子节点的QP
  switchQPs[0].toChildren.push_back(qpCounter++);  // 到右子节点的QP
  
  // 为其余交换机分配QP号
  for (int i = 1; i < 31; i++) {
    // 到父节点的QP
    switchQPs[i].toParent = qpCounter++;
    
    // 非叶子交换机有到子节点的QP
    if (i < 15) {
      switchQPs[i].toChildren.push_back(qpCounter++);  // 到左子节点的QP
      switchQPs[i].toChildren.push_back(qpCounter++);  // 到右子节点的QP
    } else {
      // 叶子交换机连接到主机
      switchQPs[i].toChildren.push_back(qpCounter++);  // 到左主机的QP
      switchQPs[i].toChildren.push_back(qpCounter++);  // 到右主机的QP
    }
  }
  
  // 主机的QP号
  uint16_t hostQPs[32];
  for (int i = 0; i < 32; i++) {
    hostQPs[i] = qpCounter++;
  }
  
  // 创建和配置31个交换机
  Ptr<IncSwitch> switches[31];
  
  for (int i = 0; i < 31; i++) {
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
      int interfaceIdx = 2 * parentIdx + childPosition;  // 计算设备索引
      
      linkState.push_back(std::make_tuple(
        interfaces[interfaceIdx].GetAddress(1),  // 当前交换机的IP
        switchQPs[i].toParent,                  // 当前交换机到父节点的QP
        interfaces[interfaceIdx].GetAddress(0),  // 父交换机的IP
        switchQPs[parentIdx].toChildren[childPosition],  // 父交换机到当前交换机的QP
        false                                   // to_father_or_son=false
      ));
    }
    
    // 添加到子节点的链路
    for (size_t j = 0; j < switchQPs[i].toChildren.size(); j++) {
      int childIdx = 2 * i + 1 + j;  // 子节点的索引
      int interfaceIdx = 2 * i + j;  // 计算设备索引
      
      if (childIdx < 31) {
        // 子节点是交换机
        linkState.push_back(std::make_tuple(
          interfaces[interfaceIdx].GetAddress(0),  // 当前交换机的IP
          switchQPs[i].toChildren[j],              // 当前交换机到子节点的QP
          interfaces[interfaceIdx].GetAddress(1),  // 子交换机的IP
          switchQPs[childIdx].toParent,           // 子交换机到当前交换机的QP
          true                                    // to_father_or_son=true
        ));
      } else {
        // 子节点是主机
        int hostIdx = childIdx - 31;  // 主机的索引
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
  
  // 创建并配置32个主机上的INC协议栈
  Ptr<IncStack> incStacks[32];
  
  for (int i = 0; i < 32; i++) {
    incStacks[i] = CreateObject<IncStack>();
    hostNodes.Get(i)->AddApplication(incStacks[i]);
    incStacks[i]->SetStartTime(Seconds(1.0));
    incStacks[i]->SetStopTime(Seconds(10000.0));
    
    std::ostringstream hostId;
    hostId << "Host" << (i+1);
    incStacks[i]->SetServerId(hostId.str());
    
    // 确定连接到主机的交换机编号
    int switchIdx = i / 2 + 15;  // 主机1-2连接到交换机16，主机3-4连接到交换机17，以此类推
    int hostPosition = i % 2;   // 0或1，表示是交换机的左或右子节点
    
    // 确定接口的索引
    int interfaceIdx = 30 + i;  // 叶子交换机到主机的链路从索引30开始
    
    incStacks[i]->SetRemote(interfaces[interfaceIdx].GetAddress(0), switchQPs[switchIdx].toChildren[hostPosition]);
    incStacks[i]->SetLocal(interfaces[interfaceIdx].GetAddress(1), hostQPs[i]);
    incStacks[i]->SetCompleteCallback(MakeBoundCallback(&AllReduceCompletionCallback, hostId.str()));
    incStacks[i]->SetWindowSize(windowSize);
    incStacks[i]->SetOperation(IncHeader::SUM);
    incStacks[i]->SetDataType(IncHeader::INT32);
    incStacks[i]->SetTotalPackets(dataSize);
    incStacks[i]->SetFillValue(1); // 所有主机的测试数据值设为1
    incStacks[i]->SetGroupId(1);   // 组ID设为1
    
    // 打印主机的IP地址
    NS_LOG_INFO("主机" << (i+1) << " IP地址: " << interfaces[interfaceIdx].GetAddress(1));
  }
  
  // 简化交换机接口打印，只显示第一个接口
  for (int i = 0; i < 31; i++) {
    Ptr<Ipv4> ipv4 = switchNodes.Get(i)->GetObject<Ipv4>();
    if (ipv4->GetNInterfaces() > 1) {
      Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1, 0);
      NS_LOG_INFO("交换机" << (i+1) << " IP地址: " << iaddr.GetLocal());
    }
  }
  
  NS_LOG_INFO("启动配置完成，将在2秒后开始AllReduce操作");
  
  // 同时启动各个主机的AllReduce操作
  for (int i = 0; i < 32; i++) {
    Simulator::Schedule(Seconds(2.0), &IncStack::AllReduce, incStacks[i]);
  }
  
  // 启动模拟
  NS_LOG_INFO("开始运行仿真...");
  
  // 设置Pcap跟踪
  p2p.EnablePcapAll("inc-topology-tree-32hosts", false);
  
  Simulator::Run();
  Simulator::Destroy();
  
  NS_LOG_INFO("仿真结束");
  
  return 0;
} 