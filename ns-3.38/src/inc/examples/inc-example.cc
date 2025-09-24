#include "ns3/core-module.h"
#include "ns3/inc-helper.h"
#include "ns3/inc-header.h"
#include "ns3/packet.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"

/**
 * \file
 *
 * 演示在网计算协议头部的创建和使用。
 */

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("IncExample");

int
main(int argc, char* argv[])
{
    bool verbose = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell application to log if true", verbose);

    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("IncExample", LOG_LEVEL_INFO);
    }

    // 创建一个在网计算协议头部
    IncHeader header;
    
    // 设置头部字段
    header.SetSrcQP(101);
    header.SetDstQP(202);
    header.SetSrcAddr(Ipv4Address("10.1.1.1"));
    header.SetDstAddr(Ipv4Address("10.1.1.2"));
    header.SetPsn(1234);
    header.SetOperation(IncHeader::SUM);
    header.SetDataType(IncHeader::INT32);
    header.SetFlag(IncHeader::SYNC);
    header.SetCwnd(50);
    header.SetGroupId(10);
    header.SetLength(1024);
    
    // 创建数据包并添加头部
    Ptr<Packet> packet = Create<Packet>(1000); // 创建1000字节的数据包
    packet->AddHeader(header);
    
    NS_LOG_INFO("Created packet with IncHeader, total size: " << packet->GetSize() << " bytes");
    
    // 模拟数据包的接收和解析
    IncHeader rxHeader;
    packet->RemoveHeader(rxHeader);
    
    // 输出解析后的头部信息
    NS_LOG_INFO("Received header: srcQP=" << rxHeader.GetSrcQP() 
               << ", dstQP=" << rxHeader.GetDstQP()
               << ", src=" << rxHeader.GetSrcAddr()
               << ", dst=" << rxHeader.GetDstAddr()
               << ", PSN=" << rxHeader.GetPsn()
               << ", op=" << static_cast<uint32_t>(rxHeader.GetOperation())
               << ", payload size=" << packet->GetSize() << " bytes");
    
    NS_LOG_INFO("Example completed successfully");

    return 0;
}
