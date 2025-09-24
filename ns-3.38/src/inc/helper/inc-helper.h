#ifndef INC_HELPER_H
#define INC_HELPER_H

#include "ns3/inc.h"
#include "ns3/inc-header.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/application-container.h"
#include "ns3/application.h"
#include "ns3/attribute.h"

namespace ns3
{

/**
 * \ingroup inc
 * \brief 在网计算协议的帮助类
 * 
 * 提供简化在网计算应用创建的方法
 */
class IncHelper
{
public:
  /**
   * \brief 创建一个IncHelper实例
   */
  IncHelper();

  /**
   * \brief 设置属性
   * \param name 属性名称
   * \param value 属性值
   */
  void SetAttribute(std::string name, const AttributeValue &value);

  /**
   * \brief 在指定节点上安装应用
   * \param node 要安装应用的节点
   * \returns 创建的应用指针
   */
  Ptr<Application> Install(Ptr<Node> node) const;

  /**
   * \brief 在节点容器中的每个节点上安装应用
   * \param nodes 要安装应用的节点容器
   * \returns 创建的应用容器
   */
  ApplicationContainer Install(NodeContainer nodes) const;

private:
  ObjectFactory m_factory; // 用于创建对象的工厂
};

} // namespace ns3

#endif /* INC_HELPER_H */
