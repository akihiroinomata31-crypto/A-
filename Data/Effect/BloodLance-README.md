# 玩家2 BloodLance 必杀技

来源：BloodLance.efkproj。游戏读取 BloodLance.efkefc，保留原工程。
8项依赖（ShortLance模型及7张贴图）全部齐全后导出，避免缺图时导出导致UV错误。
可运行 Export-BloodLance.ps1 再次导出；支持 -EffekseerExe 指定已安装的工具。

玩家1仍使用 swordMega；玩家2使用血枪环阵。倍率20，播放速度2，最长195个更新帧。
48–120帧对人物周围半径220的敌人判定，每次施放每个敌人最多承受一次2点伤害。
原按键不变：玩家2小键盘0、手柄Y。结算计数与扣分规则保留。
--player-smoke 检查资源、启动、伤害窗口、单次命中和结束清理；预览截图确认红色光圈及实体血枪正常。
