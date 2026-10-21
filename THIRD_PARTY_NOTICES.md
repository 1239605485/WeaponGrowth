# 来源与许可

工程整体沿用附带的 AGPL-3.0-or-later 许可，运行时没有链接或嵌入 Lua。

- vendor/tefkernel、vendor/kernel：TEFKernel / KernelLoader API 头文件，MIT，见 LICENSES。
- vendor/json.hpp：nlohmann/json，MIT，见 LICENSES/MIT-json.txt。
- icon.png、生成的武器目录及成长参数来自用户提供的全武器成长 v1.8.0，沿用原来源，不另行主张权利。

原先改编自 LuaLoader 的抽奖绘制代码，以及 Resources/ui、xdl、stb_image 已从本版源码和安装资源删除。

KernelLoader 接口参考提交：9009f30e468d6e084877d8c40c7c68f80262478e。

- tests/fixtures/game_metadata.hpp：从用户提供的 1.4.5.8 IL2CPP C# 转储提取的声明信息，仅用于主机兼容性测试，不包含游戏方法体，不进入 Android 动态库。

SDK 头文件仅保留实际引用部分；Windows MinGW 导出宏作兼容修正，原许可保留。
