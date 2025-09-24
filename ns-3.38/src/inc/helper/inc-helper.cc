#include "inc-helper.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/application-container.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/boolean.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("IncHelper");

IncHelper::IncHelper()
{
    // 设置默认工厂属性
    // 具体的在网计算应用类将在此处进行设置
    m_factory.SetTypeId("ns3::Application"); // 临时占位,实际应该使用在网计算应用的TypeId
}

void 
IncHelper::SetAttribute(std::string name, const AttributeValue &value)
{
    m_factory.Set(name, value);
}

Ptr<Application>
IncHelper::Install(Ptr<Node> node) const
{
    // 创建应用实例
    Ptr<Application> app = m_factory.Create<Application>();
    // 添加到节点
    node->AddApplication(app);
    
    return app;
}

ApplicationContainer
IncHelper::Install(NodeContainer nodes) const
{
    ApplicationContainer apps;
    
    for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); ++i)
    {
        apps.Add(Install(*i));
    }
    
    return apps;
}

}
