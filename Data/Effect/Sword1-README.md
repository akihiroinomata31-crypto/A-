# 普通攻击特效 Sword1

Sword1.efkproj 保留为编辑源文件，游戏加载 Sword1.efkefc。
Texture/Line01.png 与 Texture/Particle01.png 是工程引用的依赖，需一起保留。

普通攻击三段连击均播放该特效；新一段开始时重播，按角色朝向放在身前，跟随角色位置。两名玩家各自管理播放句柄，播放结束自动清理。重攻击与必杀保持现有特效。

可在 Project10/player.h 调整 PLAYER_NORMAL_ATTACK_EFFECT_MAGNIFICATION（当前 20）。
当前身前偏移 85、高度为角色碰撞高度的 55%，在 Project10/player.cpp 的 PositionNormalAttackEffect 中调整。

修改源工程后需重新导出。使用本机 Effekseer 的 -cui -in Sword1.efkproj -o Sword1.efkefc 参数可更新游戏文件。
验证：Debug x86/x64 编译通过；两个版本均通过加载、三段连击重播、双人独立播放和自然结束清理测试。
