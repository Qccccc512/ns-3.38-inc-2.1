# INC
Resources for simulating INC protocols on NS3

### Usage

This repo folder should be `<ns3-path>/src/inc/`.

当前实现了基于UDP的终结模式在网聚合协议v2.1（该版本未分离聚合号数组和广播号数组，窗口大小与聚合树的高度相关）

实现了作为对比基准的基于TCP的Ring AllReduce

原本计划在存在链路错误的情况下进行一些实验，需要可靠传输机制。在网聚合协议保证可靠传输，Ring AllReduce直接使用TCP保证可靠传输。

但考虑现实中集群的错误率很低，本地虚拟机性能不足以在接近现实集群错误率的条件下进行有意义的模拟（需要足够大的数据量）。

目前只在错误率很大的情况下（大于等于1%）验证了在网聚合和Ring AllReduce可靠传输机制的正确性。

无错误率情况下的实现，可修改Ring AllReduce使用UDP传输。

终结模式在网聚合v2.2的聚合号、广播号分离，有待进一步修改。