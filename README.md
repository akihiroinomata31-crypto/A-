# Ateam

当前唯一主工程：`Ateam.sln`，源码位于 `Project10/`。

双击 `build/bin/Win32/Release/Project10.exe` 启动游戏。
修改源码后，需要打开 `Ateam.sln`，选择 Release / x86 重新编译。编译需要 Visual Studio 的 C++ 桌面开发工具。
请保留 exe 同目录的 `Project10.assets.txt`，以及项目中的游戏素材目录。

所有新构建统一输出到 `build/`，不再维护单独的试玩版、影子版或防御版。
当前源码包含队友更新及本地自动防御、冲刺、击退等玩法修改。

`Data/`、`Player/` 为游戏素材，`DXLib/`、`ThirdParty/` 为依赖，`docs/` 保留企划资料。
已提交源码的历史由 Git 保存；旧备份、临时合并文件和旧构建已移入回收站，可恢复。
`.vs/` 是 Visual Studio 缓存，部分文件被编辑器占用，暂时保留。
