# 玩家2雷电必杀技

编辑源 Sword_Lightning.efkproj，运行时读取 Sword_Lightning.efkefc。
模型与贴图从本机 Effekseer Sample/01_Pierre02 的同名配套资源复制，保持原工程不变。

玩家1继续使用 swordMega；玩家2使用雷电长剑。雷电资源倍率12，播放速度6，最长174个游戏更新帧；22–72帧为前方剑击，73–110帧为雷击范围，半径240。每次施放每个目标最多受伤一次，伤害2，结算扣分规则不变。
雷电特效以玩家脚下为原点，不使用玩家1工程的偏移补偿。使用原工程亮度，未额外降低透明度。菜单重开时保留各自特效配置。

导出：Effekseer.exe -cui -in Sword_Lightning.efkproj -o Sword_Lightning.efkefc
验证：--player-smoke 含 P2 Lightning 加载、启动、伤害窗口、单次命中与结束清理。

2026-10-15 显示修复：最初在缺少贴图时导出的运行文件含有错误的纹理采样坐标；仅补齐PNG不足以修复，必须重新导出。现在已在20项依赖齐全后重新导出，游戏截图确认蓝色光晕、细雷电和雷击正常，方块消失。使用 Export-SwordLightning.ps1 导出，该脚本会先验证所有依赖。
