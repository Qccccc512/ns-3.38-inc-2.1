// Include a header file from your module to test.
#include "ns3/inc.h"
#include "ns3/inc-header.h"

// An essential include is test.h
#include "ns3/test.h"
#include "ns3/packet.h"
#include "ns3/buffer.h"
#include "ns3/ipv4-address.h"

// Do not put your test classes in namespace ns3.  You may find it useful
// to use the using directive to access the ns3 namespace directly
using namespace ns3;

// Add a doxygen group for tests.
// If you have more than one test, this should be in only one of them.
/**
 * \defgroup inc-tests Tests for inc
 * \ingroup inc
 * \ingroup tests
 */

// This is an example TestCase.
/**
 * \ingroup inc-tests
 * Test case for feature 1
 */
class IncTestCase1 : public TestCase
{
  public:
    IncTestCase1();
    virtual ~IncTestCase1();

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
IncTestCase1::IncTestCase1()
    : TestCase("Inc test case (does nothing)")
{
}

// This destructor does nothing but we include it as a reminder that
// the test case should clean up after itself
IncTestCase1::~IncTestCase1()
{
}

//
// This method is the pure virtual method from class TestCase that every
// TestCase must implement
//
void
IncTestCase1::DoRun()
{
    // A wide variety of test macros are available in src/core/test.h
    NS_TEST_ASSERT_MSG_EQ(true, true, "true doesn't equal true for some reason");
    // Use this one for floating point comparisons
    NS_TEST_ASSERT_MSG_EQ_TOL(0.01, 0.01, 0.001, "Numbers are not equal within tolerance");
}

/**
 * \ingroup inc-tests
 * Test case for IncHeader
 */
class IncHeaderTestCase : public TestCase
{
  public:
    IncHeaderTestCase();
    virtual ~IncHeaderTestCase();

  private:
    void DoRun() override;
};

IncHeaderTestCase::IncHeaderTestCase()
    : TestCase("IncHeader test case")
{
}

IncHeaderTestCase::~IncHeaderTestCase()
{
}

void
IncHeaderTestCase::DoRun()
{
    // 创建一个IncHeader对象并设置字段
    IncHeader header;
    
    header.SetSrcQP(1001);
    header.SetDstQP(2002);
    header.SetSrcAddr(Ipv4Address("192.168.1.1"));
    header.SetDstAddr(Ipv4Address("192.168.1.2"));
    header.SetPsn(12345);
    header.SetOperation(IncHeader::SUM);
    header.SetDataType(IncHeader::INT32);
    header.SetFlag(IncHeader::ACK);
    header.SetCwnd(100);
    header.SetGroupId(5);
    header.SetLength(1024);
    
    // 创建一个数据包,添加头部
    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(header);
    
    // 从数据包中移除头部
    IncHeader receivedHeader;
    packet->RemoveHeader(receivedHeader);
    
    // 验证字段值是否正确
    NS_TEST_ASSERT_MSG_EQ(receivedHeader.GetSrcQP(), 1001, "Wrong SrcQP");
    NS_TEST_ASSERT_MSG_EQ(receivedHeader.GetDstQP(), 2002, "Wrong DstQP");
    NS_TEST_ASSERT_MSG_EQ(receivedHeader.GetSrcAddr(), Ipv4Address("192.168.1.1"), "Wrong SrcAddr");
    NS_TEST_ASSERT_MSG_EQ(receivedHeader.GetDstAddr(), Ipv4Address("192.168.1.2"), "Wrong DstAddr");
    NS_TEST_ASSERT_MSG_EQ(receivedHeader.GetPsn(), 12345u, "Wrong PSN");
    NS_TEST_ASSERT_MSG_EQ(receivedHeader.GetOperation(), IncHeader::SUM, "Wrong Operation");
    NS_TEST_ASSERT_MSG_EQ(receivedHeader.GetDataType(), IncHeader::INT32, "Wrong DataType");
    NS_TEST_ASSERT_MSG_EQ(receivedHeader.HasFlag(IncHeader::ACK), true, "ACK flag not set");
    NS_TEST_ASSERT_MSG_EQ(receivedHeader.GetCwnd(), 100, "Wrong Cwnd");
    NS_TEST_ASSERT_MSG_EQ(receivedHeader.GetGroupId(), 5, "Wrong GroupId");
    NS_TEST_ASSERT_MSG_EQ(receivedHeader.GetLength(), 1024, "Wrong Length");
}

// The TestSuite class names the TestSuite, identifies what type of TestSuite,
// and enables the TestCases to be run.  Typically, only the constructor for
// this class must be defined

/**
 * \ingroup inc-tests
 * TestSuite for module inc
 */
class IncTestSuite : public TestSuite
{
  public:
    IncTestSuite();
};

IncTestSuite::IncTestSuite()
    : TestSuite("inc", UNIT)
{
    // TestDuration for TestCase can be QUICK, EXTENSIVE or TAKES_FOREVER
    AddTestCase(new IncTestCase1, TestCase::QUICK);
    AddTestCase(new IncHeaderTestCase, TestCase::QUICK);
}

// Do not forget to allocate an instance of this TestSuite
/**
 * \ingroup inc-tests
 * Static variable for test initialization
 */
static IncTestSuite sincTestSuite;
