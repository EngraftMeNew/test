本仓库用于中国科学院大学《操作系统原理》课程 Project1 实验，  

主要目标是构建并验证一个简化的 RISC-V 操作系统启动与加载流程。

仓库结构：  
.  
├── arch/            # 架构相关代码（RISC-V 启动、汇编、异常处理）  
├── build/           # 编译输出目录（可执行镜像、临时文件）  
├── include/         # 公共头文件  
├── init/            # 初始化与入口代码  
├── kernel/loader/   # 内核加载逻辑  
├── libs/            # 基础库（string、printf 等）  
├── test/test_project1/ # 测试用例  
├── tools/           # 镜像打包与辅助脚本  
├── Makefile         # 主 Makefile  
└── README.md        # 项目说明  

运行命令：  

cd tools/  && gcc -o createimage createimage.c && cd ..  

make all  

make run  