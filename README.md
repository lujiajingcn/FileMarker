# FileMarker —— 文件标签管理工具

给任意文件打标签，按标签快速找回来。标签直接写在文件的 **NTFS 数据流（ADS）** 里，
不移动文件、不改文件名、不依赖数据库，拷走文件标签就跟着走（NTFS → NTFS）。

- 语言：C++ / Qt 5.13（MinGW 64-bit）
- 平台：Windows（依赖 NTFS 数据流）
- 版本：1.0.0

---

## 主要功能

| 功能 | 说明 |
|---|---|
| 标签库 | 树形标签，支持新建/新建子标签/重命名/删除/上下左右移动，保存在 `labels.xml` |
| 给文件打标签 | 选中文件 → 「添加标签」；或把标签从标签库**拖到文件上**（拖到某个文件=只给它加） |
| 标签 chip | 详情/缩略图两种视图都显示带 `✕` 的药丸标签，点 `✕` 直接删标签（可 Ctrl+Z 撤销） |
| 搜索 | 按标签组搜索：组内 OR（父标签自动包含子标签），组间 AND/OR；数据来源可选缓存或磁盘 |
| 搜索缓存 | sqlite 缓存目录的标签结果，结果页显示「缓存是多久之前的」，可一键**重新扫描** |
| 标签备份 | 导出/导入 JSON 清单，防止标签在 FAT32 U 盘、压缩包、网盘上丢失 |
| AI 自动打标 | 调用外部 `extract_and_tag.py`（需要本机 Python 环境）批量生成标签 |
| 单实例 | 同时只允许一个实例运行，第二次启动会把已有窗口叫到前台 |

### 快捷键

| 按键 | 作用 |
|---|---|
| `Ctrl+F` | 用当前选中的标签查找文件 |
| `F3` / `Ctrl+Shift+F` | 弹出「查找条件」菜单（标签关系 / 数据来源） |
| `Ctrl+A` | 全选当前目录的文件（地址栏里仍是全选文本） |
| `Enter` | 打开文件 / 进入目录 |
| `F2` | 重命名选中的文件 |
| `Delete` / `Ctrl+Shift+C` | 清空选中文件的标签（有确认框） |
| `Ctrl+Z` | 撤销上一次标签操作 |

---

## 数据放在哪

| 文件 | 位置 | 内容 |
|---|---|---|
| 标签本身 | 每个文件的 NTFS 数据流（流名=标签名，流内容=顺序号） | 归属清单 `FileMarker.Owned` 记录哪些流是本程序写的 |
| 标签库 | `<程序目录>/labels.xml` | 树形结构 + UUID |
| 搜索缓存 | `<程序目录>/FileMarker.db` | 目录遍历结果的 sqlite 缓存 |
| 设置 | `<程序目录>/FileMarker.ini` | 视图、列宽、查找条件等 |
| 日志 | `<程序目录>/FileMarker.log` | 运行日志 |
| 标签备份 | 用户选的 `.json` 文件 | 路径 → 标签列表（含相对路径，目录搬走也能找回） |

> 第三方数据流（例如 Windows 自己写的 `Zone.Identifier`）不会被当成标签，也不会被删除。

---

## 构建

```powershell
# 依赖：Qt 5.13.0 MinGW 64-bit（含 mingw730_64 工具链）
$env:PATH = "E:\V_C\Qt\Qt5.13.0\5.13.0\mingw73_64\bin;E:\V_C\Qt\Qt5.13.0\Tools\mingw730_64\bin;" + $env:PATH

mkdir build-review; cd build-review
qmake ..\FileMarker.pro -spec win32-g++ CONFIG+=debug
mingw32-make -f Makefile.Debug -j4
```

产物：`build-review/debug/FileMarker.exe`（运行需要 PATH 里有 Qt `bin` 与 `FileMarker/lib`）。

> 加了/删了源文件之后**必须重新跑 qmake**，否则 Makefile 里没有新文件。

## 运行

```powershell
$env:PATH = "<Qt>\bin;<工程目录>\FileMarker\lib;" + $env:PATH
.\build-review\debug\FileMarker.exe
```

## 单元测试

```powershell
mkdir tests\build; cd tests\build
qmake ..\tests.pro -spec win32-g++ CONFIG+=debug
mingw32-make -f Makefile.Debug -j4
.\debug\tst_filemarker.exe
```

覆盖 5 组、43 个用例：

| 测试类 | 覆盖内容 |
|---|---|
| `tst_adsoperation` | 标签流读写（顺序/追加去重/改名/删除/清空/第三方流保留） |
| `tst_xmloperation` | 标签库 XML 往返、旧格式兼容、损坏文件兜底 |
| `tst_searchmatch` | 组内 OR / 组间 AND-OR、父标签含子标签、共享匹配函数 |
| `tst_utility` | 标签拆分（中英文分隔符、去重）、流名判断 |
| `tst_tagexport` | JSON 往返、覆盖/追加写回、路径失效后的相对路径找回 |

## 打包绿色版

```powershell
powershell -ExecutionPolicy Bypass -File package.ps1              # Release
powershell -ExecutionPolicy Bypass -File package.ps1 -Config Debug
```

流程：拷 exe → `windeployqt` 补 Qt 运行库 → 拷 `sqlite3.dll` / `extract_and_tag.py` /
`translations/qt_zh_CN.qm` → 写使用说明 → 压成 `dist/FileMarker-<时间戳>.zip`。
默认构建目录是 `..\build-review`（相对 `.pro` 所在目录）。

---

## 目录结构

```
FileMarker.pro              工程文件
main.cpp                    入口：高 DPI、日志接管、单实例锁、Qt 中文翻译
mainwindow.*                主窗口：菜单、停靠面板、日志、AI 环境自检

formpagefiles.cpp           文件页主干：两个视图的初始化、选择、刷新
formpagefilesnav.cpp          导航：前进/后退/上级、地址栏历史、双击打开
formpagefilestags.cpp         标签：编辑/删除、目录标签扫描、重命名
formpagefilesmenu.cpp         菜单：右键菜单、排序、视图切换、命令条
formpagefilesutil.h           共用小工具（"我的电脑"判断、路径比较）

formfilebrowser.cpp         文件浏览页主干：命令条、导航面板、页面切换、线程控制
formfilebrowsersearch.cpp     搜索：缓存/磁盘、重新扫描、遍历、停止
formfilebrowsertagops.cpp     标签操作：添加/删除、撤销、AI 打标
formfilebrowserexport.cpp     标签导出 / 导入

formlabels.cpp              标签库主干：模型、XML 持久化、基础查询
formlabelstree.cpp            树操作：增删改、移动、改名同步到文件
formlabelsmisc.cpp            菜单、拖拽、外部标签回传

adsoperation.*              NTFS 数据流读写（标签 + 归属清单）
xmloperation.*              标签库 XML 读写（QXmlStreamReader/Writer）
sqliteoperation.*           搜索缓存（sqlite）
tagexport.*                 标签备份 JSON
searchmatch.*               搜索匹配规则的唯一实现（缓存与磁盘共用）
myqfilesystemmodel.*        文件模型 + 标签列缓存
tagchipdelegate.*           详情视图标签 chip
editablefiledelegate.*      缩略图视图（图标 + 文件名 + chip）
thumbnailiconprovider.*     缩略图生成与缓存
threadsearch.cpp            磁盘扫描线程
threadtraversedirs.cpp      目录遍历线程
threadaddtagbyai.cpp        AI 打标线程

tests/                      单元测试（Qt Test）
package.ps1                 一键打包
```

---

## 开发约定

- **提交前**跑一遍单元测试与全量重建，确保 0 错误 0 警告。
- 注释用中文、写"为什么"而不是"做了什么"；不要留 `【C12】`/`P2-21` 这类评审编号。
- 标签相关的写盘只走 `ADSOperation`，不要在别处直接拼 `path:stream`。
- 搜索匹配只走 `SearchMatch::matchesFileLabels()`，缓存和磁盘两条路径共用，避免规则漂移。

## 已知限制

- 只支持 NTFS：FAT32/exFAT 的 U 盘、压缩包、网盘同步会丢标签 → 用「导出标签」备份。
- 标签名不能含 `: / \ * ? " < > |`（会被规范化），大小写不敏感。
- 缩略图依赖系统 Shell 缩略图，少数格式只能显示图标。
- AI 打标需要本机 Python 与脚本依赖，程序只负责调用。
