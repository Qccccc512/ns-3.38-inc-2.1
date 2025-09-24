#include "inc-header.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("IncHeader");
NS_OBJECT_ENSURE_REGISTERED(IncHeader);

IncHeader::IncHeader()
    : m_srcQP(0)
    , m_dstQP(0)
    , m_psn(0)
    , m_operation(SUM)
    , m_typeAndFlags(0)
    , m_cwnd(0)
    , m_groupId(0)
    , m_length(0)
    , m_aggDataTest(0)
{
    // 设置默认数据类型为INT32
    SetDataType(INT32);
}

IncHeader::~IncHeader()
{
}

TypeId
IncHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::IncHeader")
        .SetParent<Header>()
        .SetGroupName("Applications")
        .AddConstructor<IncHeader>();
    return tid;
}

TypeId
IncHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

void
IncHeader::Print(std::ostream &os) const
{
    os << "srcQP=" << m_srcQP
       << " dstQP=" << m_dstQP
       << " src=" << m_srcAddr
       << " dst=" << m_dstAddr
       << " psn=" << m_psn
       << " op=" << static_cast<uint32_t>(m_operation)
       << " datatype=" << static_cast<uint32_t>(GetDataType())
       << " flags=0x" << std::hex << (GetFlags() & 0x0F) << std::dec
       << " cwnd=" << m_cwnd
       << " groupId=" << m_groupId
       << " length=" << m_length
       << " aggDataTest=" << m_aggDataTest;
}

uint32_t
IncHeader::GetSerializedSize() const
{
    // 计算协议头总长度:
    // - srcQP (2 bytes)
    // - dstQP (2 bytes)
    // - srcAddr (4 bytes)
    // - dstAddr (4 bytes)
    // - psn (4 bytes)
    // - operation (1 byte)
    // - typeAndFlags (1 byte)
    // - cwnd (2 bytes)
    // - groupId (2 bytes)
    // - length (2 bytes)
    // - aggDataTest (4 bytes)
    return 28; // 总共28字节
}

void
IncHeader::Serialize(Buffer::Iterator start) const
{
    // 写入源QP和目的QP
    start.WriteHtonU16(m_srcQP);
    start.WriteHtonU16(m_dstQP);
    
    // 写入源地址和目的地址
    start.WriteHtonU32(m_srcAddr.Get());
    start.WriteHtonU32(m_dstAddr.Get());
    
    // 写入序列号
    start.WriteHtonU32(m_psn);
    
    // 写入操作类型
    start.WriteU8(static_cast<uint8_t>(m_operation));
    
    // 写入数据类型和标志位组合
    start.WriteU8(m_typeAndFlags);
    
    // 写入拥塞窗口
    start.WriteHtonU16(m_cwnd);
    
    // 写入组ID
    start.WriteHtonU16(m_groupId);
    
    // 写入总长度
    start.WriteHtonU16(m_length);
    
    // 写入聚合测试数据
    start.WriteHtonU32(static_cast<uint32_t>(m_aggDataTest));
}

uint32_t
IncHeader::Deserialize(Buffer::Iterator start)
{
    // 读取源QP和目的QP
    m_srcQP = start.ReadNtohU16();
    m_dstQP = start.ReadNtohU16();
    
    // 读取源地址和目的地址
    m_srcAddr.Set(start.ReadNtohU32());
    m_dstAddr.Set(start.ReadNtohU32());
    
    // 读取序列号
    m_psn = start.ReadNtohU32();
    
    // 读取操作类型
    m_operation = static_cast<Operation>(start.ReadU8());
    
    // 读取数据类型和标志位组合
    m_typeAndFlags = start.ReadU8();
    
    // 读取拥塞窗口
    m_cwnd = start.ReadNtohU16();
    
    // 读取组ID
    m_groupId = start.ReadNtohU16();
    
    // 读取总长度
    m_length = start.ReadNtohU16();
    
    // 读取聚合测试数据
    m_aggDataTest = static_cast<int32_t>(start.ReadNtohU32());

    return GetSerializedSize();
}

void
IncHeader::SetSrcQP(uint16_t qp)
{
    m_srcQP = qp;
}

uint16_t
IncHeader::GetSrcQP() const
{
    return m_srcQP;
}

void
IncHeader::SetDstQP(uint16_t qp)
{
    m_dstQP = qp;
}

uint16_t
IncHeader::GetDstQP() const
{
    return m_dstQP;
}

void
IncHeader::SetSrcAddr(Ipv4Address addr)
{
    m_srcAddr = addr;
}

Ipv4Address
IncHeader::GetSrcAddr() const
{
    return m_srcAddr;
}

void
IncHeader::SetDstAddr(Ipv4Address addr)
{
    m_dstAddr = addr;
}

Ipv4Address
IncHeader::GetDstAddr() const
{
    return m_dstAddr;
}

void
IncHeader::SetPsn(uint32_t psn)
{
    m_psn = psn;
}

uint32_t
IncHeader::GetPsn() const
{
    return m_psn;
}

void
IncHeader::SetOperation(Operation op)
{
    m_operation = op;
}

IncHeader::Operation
IncHeader::GetOperation() const
{
    return m_operation;
}

void
IncHeader::SetDataType(DataType dataType)
{
    // 清除高4位，然后设置数据类型到高4位
    m_typeAndFlags = (m_typeAndFlags & 0x0F) | (static_cast<uint8_t>(dataType) << 4);
}

IncHeader::DataType
IncHeader::GetDataType() const
{
    // 从高4位提取数据类型
    return static_cast<DataType>((m_typeAndFlags >> 4) & 0x0F);
}

void
IncHeader::SetFlags(uint8_t flags)
{
    // 只保留低4位的标志位
    m_typeAndFlags = (m_typeAndFlags & 0xF0) | (flags & 0x0F);
}

uint8_t
IncHeader::GetFlags() const
{
    // 返回低4位的标志位
    return m_typeAndFlags & 0x0F;
}

void
IncHeader::SetCwnd(uint16_t cwnd)
{
    m_cwnd = cwnd;
}

uint16_t
IncHeader::GetCwnd() const
{
    return m_cwnd;
}

void
IncHeader::SetGroupId(uint16_t groupId)
{
    m_groupId = groupId;
}

uint16_t
IncHeader::GetGroupId() const
{
    return m_groupId;
}

void
IncHeader::SetLength(uint16_t length)
{
    m_length = length;
}

uint16_t
IncHeader::GetLength() const
{
    return m_length;
}

void
IncHeader::SetAggDataTest(int32_t value)
{
    m_aggDataTest = value;
}

int32_t
IncHeader::GetAggDataTest() const
{
    return m_aggDataTest;
}

void
IncHeader::SetFlag(FlagBits flag)
{
    // 只设置低4位的标志位
    m_typeAndFlags |= (flag & 0x0F);
}

void
IncHeader::UnsetFlag(FlagBits flag)
{
    // 只清除低4位的标志位
    m_typeAndFlags &= ~(flag & 0x0F);
}

bool
IncHeader::HasFlag(FlagBits flag) const
{
    // 检查低4位的标志位
    return (m_typeAndFlags & (flag & 0x0F)) != 0;
}

} // namespace ns3 