# SwordMega 必杀技

编辑源：swordMega.efkproj；游戏读取：swordMega.efkefc。Texture 中的配套贴图必须一起保留。

倍率 32（比原先放大 60%），按朝向补偿工程 X=2、Y=-4 的原始偏移，将主剑特效中心对齐角色；位移补偿随倍率自动计算。总动作时长 240 帧（60 FPS）；伤害窗口：40–69 帧为身前胶囊斩击，70–165 帧为剑光范围，从半径 160 扩张至 336，100 帧达到最大范围。166 帧后仅播放收尾，不再造成伤害。

同一次施放可命中多个敌人，每个敌人最多受到一次 2 点伤害。判定包含敌人的碰撞宽度，并检查高度范围；两名玩家分别管理命中记录。

参数与命中逻辑：Project10/player.h、Project10/player.cpp。可用 --player-smoke 验证加载、伤害窗口、正面斩击、多目标命中、范围扩张和播放清理。

重新导出：在此目录运行本机 Effekseer.exe -cui -in swordMega.efkproj -o swordMega.efkefc。


分屏投影修正：DxLib 的逻辑视口仍为 900×600，但 Direct3D 实际视口为左/右 450×600。Effekseer_Sync3DSetting 只复制相机矩阵，必须另将投影矩阵乘以逻辑视口矩阵和实际视口逆矩阵，才能与模型的屏幕位置一致。修正在 main.cpp 的 SyncEffekseerDrawAreaProjection，两个分屏均调用；--player-smoke 检查左/右/全屏下多个世界坐标的投影误差。
