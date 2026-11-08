# Sword2 普通攻击特效

游戏读取 Sword2.efkefc，编辑源为 Sword2.efkproj。

导出前必须先准备好 Texture/StanBlade.png、Texture/Thunder01.png、Texture/Burst01.png 和 Texture/t0002.png。缺失时导出仍可能成功，但绘制节点的贴图索引会变成 -1；之后仅补回 PNG 不会修复已经导出的文件，必须重新导出。

本次已在四张贴图齐全后重新导出，并检查实际渲染和四个可见节点的贴图绑定。倍率调整为 40（原来的 2 倍），播放速度为 2/3（完整播放时间为原来的 1.5 倍）。位置和方向保持不变；下一段连击仍会重新播放特效。参数位于 Project10/player.h。
