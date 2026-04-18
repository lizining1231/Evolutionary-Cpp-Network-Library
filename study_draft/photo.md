graph LR
    Root((性能优化与问题排查全记录)) --> Part1[一、第一次QPS暴跌9500 → 400+]
    Root --> Part2[二、第二次QPS暴跌10000+ → 400+]
    Root --> Part3[三、核心：参数计算推演⭐]
    Root --> Part4[四、踩坑与总结]
    Root --> Part5[五、测试环境说明]
    
    Part1 --> P1_1[1. 前提引入]
    Part1 --> P1_2[2. 三个数据现象]
    Part1 --> P1_3[3. 修复思路]
    Part1 --> P1_4[4. commit message]
    
    subgraph FirstFix[第一次修复的7个步骤]
        P1_3 --> Step1[(1) 排除1本地回环正常]
        P1_3 --> Step2[(2) 排除2docker-proxy损失5-10%]
        P1_3 --> Step3[(3) 认知错误怀疑CPU占用]
        P1_3 --> Step4[(4) 检查CPU限制]
        P1_3 --> Step5[(5) 拉回docker-proxy]
        P1_3 --> Step6[(6) 归因错误启动docker-proxy]
        P1_3 --> Step7[(7) 歪打正着清理系统]
    end
    
    Part2 --> P2_1[1. 前提引入]
    Part2 --> P2_2[2. 数据现象]
    Part2 --> P2_3[3. 修复思路]
    Part2 --> P2_4[4. commit message]
    
    Part3 --> P3_1[1. tcp_tw_reuse三态参数]
    Part3 --> P3_2[2. 利特尔定律QPS计算推演]
    Part3 --> P3_3[3. 两组对照实验]
    
    Part4 --> P4_1[1. 7个踩坑与2道面试题]
    Part4 --> P4_2[2. 个人错误与反思]
    Part4 --> P4_3[3. 如果再查一次]
    
    P4_1 --> P4_1_1[坑1: swarm deploy误用]
    P4_1 --> P4_1_2[坑2: cgroup版本兼容]
    P4_1 --> P4_1_3[坑3: %CPU反直觉]
    P4_1 --> P4_1_4[坑4: tcp_fin_timeout误区❌]
    P4_1 --> P4_1_5[坑5: 减少TIME-WAIT误区❌]
    P4_1 --> P4_1_6[坑6: tcp_tw_reuse默认关闭原因]
    P4_1 --> P4_1_7[坑7: SO_REUSEADDR ≠ tcp_tw_reuse]
    
    P4_1_4 --> Interview1[面试题1: tcp_fin_timeout能缩短TIME_WAIT吗？]
    P4_1_5 --> Interview1
    P4_1_6 --> Interview2[面试题2: 为何默认关闭tcp_tw_reuse?]
    P4_1_7 --> Interview2
    
    Part5 --> P5_1[1. 硬件环境]
    Part5 --> P5_2[2. 软件环境]
    Part5 --> P5_3[3. 参数设置]
    Part5 --> P5_4[4. 容器配置]