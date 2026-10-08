# 新玩家动作导入版

双击项目根目录的 TryNewPlayer.cmd，或在 Visual Studio 中运行 Debug。

已加入 Running.fbx 对应的新跑步动作，并从 Great Sword Pack 转换了 2 个跑步动作和 11 个攻击类动作，生成的 MV1 保留在原 FBX 旁边。已接入默认 great sword idle 待机动作；尚未接入防御、受伤、死亡等其他新动作。

## 游戏内对应

| 操作 | 素材 |
| --- | --- |
| 待机（开局、停止移动、攻击结束） | great sword idle.mv1 |
| 前进 / 侧向移动（W、A、D / 上、左、右） | Running.mv1 |
| 向下移动（S / 下，转身向前跑） | Running.mv1 |
| 普攻第 1 段（空格 / Enter） | great sword slash.mv1 |
| 普攻第 2 段 | great sword slash (5).mv1 |
| 普攻第 3 段 | great sword slash (3).mv1 |
| 重攻击（E / 右 Shift） | great sword attack.mv1 |
| 必杀（F / 右 Ctrl） | great sword high spin attack.mv1 |

其他跑步、跳斩、滑步攻击、踢击和斩击版本已转换并验证，可在 Project10/PlayerMotionAssets.h 调整对应关系。当前没有为这些备用动作增加新按键或新战斗状态。

动作的水平根位移被抵消，移动和碰撞继续跟随游戏坐标；垂直起伏及旋转保留。攻击结束时间使用各动作的实际长度，重攻击与必杀使用独立动作。

待机使用持剑待机动画并循环播放；普通跳跃、受伤、倒地仍使用先前的静态 T 姿势占位。攻击范围、特效时机和动作切换的平滑程度可继续调试。

原 FBX、旧 PC.mv1 和旧动画均保留。把 Project10/player.h 的 PLAYER_USE_NEW_MODEL 改为 false 并重新编译可恢复旧角色。主模型同目录的 .fbm 文件夹是贴图，请一起保留。

验证：Debug x86/x64 编译通过；当前使用的 13 个动作已验证加载到角色，手脚骨骼确实变化，应用水平位移抵消后根骨骼水平偏移为零。详细结果见 MOTION-IMPORT-CHECK.txt。前进/后退/侧向动作连续切换和动画不中断测试通过。所有移动均按移动方向转身，S / 下不再后退。完整操作手感仍需试玩确认。

当前所有移动均使用 Running.mv1；旧大剑跑步动作已从动作配置中移除，原文件保留。前后移动共用同一个动画句柄，切换方向时保持动画连续播放。

已删除后退判断和独立后退动作参数，所有方向共用 Running；方向切换保持动画连续播放。
