# 全武器成长 Native 2.1.0

纯 C++17 KernelLoader 模组，目标游戏 1.4.5.8。保留世界成长、职业击杀经验、越级压制、抽奖事务及旧存档迁移；没有 Lua 运行时或抽奖 UI。管理器可编辑六项设置，包含成长、抽奖、联机开关、成长模式、越级压制和迁移路径。

## 指令

| 指令 | 用途 |
| --- | --- |
| /随机单抽 | 随机单抽 |
| /指定近战单抽 | 指定职业单抽 |
| /随机职业十连 | 随机十连 |
| /指定召唤职业十连 | 指定职业十连 |
| /查看状态 | 成长、网络角色、券数和奖池 |
| /抽奖记录 | 记录，可加空格和页码 1–7 |

指定职业可以替换为近战、远程、魔法、召唤。旧 /wg 指令保留兼容。

## 合作联机

各玩家安装本模组，在自己的客户端启用 multiplayer_enabled；本地世界角色 0、客户端 1 和移动端本地主机 3 可运行，专用服务端角色 2 不应用倍率或发奖。服务器不必再安装同一模组叠加成长。各人保存自己的角色经验、抽奖券和收藏，本地玩家的击杀归因用于经验与首杀券；不奖励其他玩家的击杀。

抽奖先保存事务，再克隆物品、验证入包，联机调用游戏 NetMessage.SendData 的 SyncEquipment 同步变更槽位，最后完成账本。同步 API 缺失时拒绝抽取；调用失败冻结事务，避免重复发奖。同步调用成功表示已交给原生网络层，不代表服务器已经确认；断线后仍应核对背包。这里没有自定义服务器统一券库、经济认证、跨玩家经验共享或反作弊协议，限合作环境使用。多人时不改写共享的鞭子标记表，避免改变其他玩家的标记属性。

网络角色切换当帧清空排队指令和射弹来源；角色、世界切换重建会话。enabled 只控制成长，gacha_enabled 独立控制抽奖。growth_mode 为 world/kill/mixed，默认 world；新世界初期没有世界进度增益是正常情况。

## 平台与构建

| 平台 | 输出 |
| --- | --- |
| Android arm64 | libWeaponGrowth.android.arm64.so |
| Windows 32 位 | WeaponGrowth.windows.x86.dll |
| Windows 64 位 | WeaponGrowth.windows.x64.dll |
| Linux 32 位 | libWeaponGrowth.linux.x86.so |
| Linux 64 位 | libWeaponGrowth.linux.x64.so |

安装 NDK r26c 与 CMake/Ninja 后运行 tools/build_android.ps1。将 Zig 0.14.1 解压到 .tools/ziglang，Python 3 运行 tools/build_desktop.py；可用 --zig 指定其他路径，--platform windows 或 linux 限定目标。Linux 目标 glibc 2.17，C++ 运行库由 Zig 链接到模块内。桌面也可用 Clang/GCC + CMake，WG_BUILD_MODULE=ON；需要宿主相同位数的 KernelLoader。

Windows 运行 tools/test.ps1；其他主机可 CMake -DBUILD_TESTING=ON -DWG_BUILD_MODULE=OFF 后执行 CTest。Python tools/package.py --llvm <LLVM工具目录> 校验五个平台文件后生成安装包、源码包和校验和。源码包使用明确文件列表，不包含编译缓存、旧版本安装包、日志或工具运行时。

## 安装与存档

备份角色，停用旧模组，再导入新版并完全重启。沿用 liuxin.weapon_growth.kernel 包名和双槽账本，不清除玩家存档。Resources/config.json 是默认配置，实际配置在模组私有目录，每两秒重新检查。legacy_directory 允许指定旧版存档所在绝对目录；迁移不覆盖旧文件。

本次清理只涉及此源码工程的旧产物和未引用文件，没有删除下载目录原始 ZIP 或设备角色存档。许可见 THIRD_PARTY_NOTICES.md。

## 验证范围

2.0.3 Android arm64 已经用户确认可运行。2.1.0 七组主机测试覆盖真实 1.4.5.8 元数据、初始化/回滚、成长实际回调、中文抽奖、联网背包同步、角色切换、召唤/AI 子弹幕、事务与历史。五个平台均检查架构、导出和版本。新增联机及四个桌面目标未完成真实游戏多设备验收，因此标为实验版本。

native-runtime.log 的 init.ready、heartbeat.enter、profile.ready、heartbeat.active 分别帮助定位加载、Hook 执行、账本和世界状态。不要在事务不确定时手动删除 pending 自动补发。
