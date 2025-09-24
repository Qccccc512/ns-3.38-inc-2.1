# INC
Resources for simulating INC protocols on NS3

### Usage

This repo folder should be `<ns3-path>/src/inc/`.

当前实现了基于UDP的终结模式在网聚合协议v2.1（该版本未分离聚合号数组和广播号数组，窗口大小与聚合树的高度相关）

实现了作为对比基准的基于TCP的Ring AllReduce

协议v2.2的聚合号、广播号分离有待进一步开发

