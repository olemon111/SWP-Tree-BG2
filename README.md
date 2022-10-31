# SWP-Tree

> 一种新的动态适应各种倾斜负载的基于混合介质的持久化学习索引结构
>
> 本项目为其背景测试

### 测试目的

- 测试现有内存学习索引结构在DRAM中的性能和空间占用

### 测试方案：

- ALEX (https://github.com/microsoft/ALEX)
- LIPP (https://github.com/Jiacheng-WU/lipp)
- XIndex(https://ipads.se.sjtu.edu.cn:1312/opensource/xindex.git)
- PGM (https://github.com/gvinciguerra/PGM-index)


### 测试负载

- LLT 数据集
- uniform负载

### 测试指标

- 吞吐量(Kops/s)

- DRAM空间占用(GB of DRAM)

### 环境配置

- 2 million KVs (8B key-8B value)
- 每个测试项执行10 million指令