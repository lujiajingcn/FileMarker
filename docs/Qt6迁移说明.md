# FileMarker Qt6 迁移说明

- 迁移日期：2026-11-12
- 目标环境：**Qt 6.9.3（mingw_64）+ MinGW 13.1.0**
- 旧环境：Qt 5.13.0 + MinGW 7.3（已卸载）
- **Qt5 支持已彻底移除**（不依赖 Qt5Compat 模块）

---

## 一、构建方法

### 命令行（qmake）

```powershell
# 1) 准备环境变量（把 Qt6 和配套 MinGW13 放到 PATH 最前面）
$env:PATH = "E:\V_C\Qt\6.9.3\mingw_64\bin;E:\V_C\Qt\Tools\mingw1310_64\bin;" + $env:PATH

# 2) 影子构建（工程源码目录外）
mkdir E:\study_lujiajing\WorkSpace_FileMarker\FileMarker\build-qt6
cd  E:\study_lujiajing\WorkSpace_FileMarker\FileMarker\build-qt6
E:\V_C\Qt\6.9.3\mingw_64\bin\qmake.exe ..\FileMarker\FileMarker\FileMarker.pro -spec win32-g++ CONFIG+=debug
E:\V_C\Qt\Tools\mingw1310_64\bin\mingw32-make.exe -f Makefile.Debug -j6
```

### 单元测试

```powershell
cd E:\study_lujiajing\WorkSpace_FileMarker\FileMarker\FileMarker\tests\build-qt6
E:\V_C\Qt\6.9.3\mingw_64\bin\qmake.exe ..\tests.pro -spec win32-g++ CONFIG+=debug
E:\V_C\Qt\Tools\mingw1310_64\bin\mingw32-make.exe -f Makefile.Debug -j6
.\debug\tst_filemarker.exe
```

### Qt Creator

直接用套件 `Desktop Qt 6.9.3 MinGW 64-bit` 打开 `FileMarker.pro` 即可。
Qt Creator 生成的影子构建目录 `/build/` 已加入 `.gitignore`。

### 运行

exe 同目录需要有：

| 文件 | 说明 |
|---|---|
| `sqlite3.dll` | 来自仓库 `lib/sqlite3.dll`（链接时用 `-Llib -lsqlite3`，MinGW 的 ld 可直接链接 DLL） |
| `labels.xml` | 标签库数据（首次运行没有时标签库为空） |
| `FileMarker.db` | 搜索缓存，自动创建 |
| `FileMarker.ini` / `config.ini` | 窗口状态 / 搜索条件，自动创建 |

Qt6 本身的 DLL 可以从 `E:\V_C\Qt\6.9.3\mingw_64\bin` 取，或打包时用 `windeployqt`。

---

## 二、代码改动清单

| # | 位置 | 改动 | 原因 |
|---|---|---|---|
| 1 | `FileMarker.pro`、`tests/tests.pro` | `CONFIG += c++11` → `c++17` | Qt6 要求 C++17 |
| 2 | `paralleldirtraverser.cpp` | 删除 MinGW 7 的 `DEVICE_SEEK_PENALTY_DESCRIPTOR` / `IOCTL_STORAGE_QUERY_PROPERTY` 兜底定义 | MinGW 13 的 `winioctl.h` 已自带（重复定义会报 conflicting declaration） |
| 3 | `formlabelsmisc.cpp`、`formpagefiles.cpp`、`editablefiledelegate.cpp`、`pathbreadcrumb.cpp` | 补 `#include <QMouseEvent> / <QDropEvent> / <QDragMoveEvent> / <QUrl> / <QContextMenuEvent> / <QKeyEvent>` | Qt6 头文件不再间接包含这些类型 |
| 4 | `formpagefiles.cpp` | `Qt::CTRL + Qt::Key_X` → `Qt::CTRL \| Qt::Key_X` | Qt6 用 `QKeyCombination`，`+` 重载已删除 |
| 5 | `pathbreadcrumb.cpp` | `QString::SkipEmptyParts` → `Qt::SkipEmptyParts` | Qt6 枚举位置变更 |
| 6 | `formlabelsmisc.cpp`、`formpagefiles.cpp` | `QDropEvent::pos()` → `position().toPoint()` | Qt6 弃用 `pos()` |
| 7 | 6 个文件共 11 处 | 删除 `QSettings::setIniCodec("UTF-8")` 及其 `#if QT_VERSION < 6` 保护 | Qt6 INI 默认 UTF-8，该接口已移除 |
| 8 | `main.cpp` | 删除 `AA_EnableHighDpiScaling` / `AA_UseHighDpiPixmaps` | Qt6 默认开启高 DPI，属性已移除 |
| 9 | `main.cpp` | `QLibraryInfo::location()` → `path()`；中文翻译优先加载 `qtbase_zh_CN.qm`（回退 `qt_zh_CN.qm`） | Qt6 翻译拆分为 qtbase_*；`location` 已弃用 |
| 10 | `mainwindow.h` | 补 `#include <QFile>`（持有 `QFile m_logFile`） | Qt6 不再间接包含 |
| 11 | `.gitignore` | 忽略 `/build/`、`/tests/build-qt6/` | Qt6 新增的本地构建目录 |

> 说明：`xmloperation.cpp` 用的是 `QXmlStreamReader/Writer`（属于 QtCore），不需要额外的 QtXml 模块。

---

## 三、验证结果

| 项目 | 结果 |
|---|---|
| 主工程编译 | **0 错误 0 警告**（`QT_DEPRECATED_WARNINGS` 已开启） |
| 单元测试 | **55 / 55 通过**（6 个测试类，Qt6 构建） |
| 启动 / 主界面 | 正常；窗口几何、左侧导航、两个 dock 面板均正常 |
| 标签库 | `labels.xml` 正常读取，三个测试标签显示正常 |
| 打标签（ADS） | 正常写入（`FileMarker.Owned` + 标签流） |
| 遍历（并行线程） | 正常：磁盘判定（机械盘→1 线程）日志正常，缓存写入正常 |
| 搜索 | 正常：磁盘扫描/缓存两条路径都能返回结果 |
| 缩略图 | 正常：Shell 缩略图（PNG/JPG/文本）与标签芯片都能绘制 |
| 中文翻译 | 正常：`qtbase_zh_CN.qm` 加载成功，标准按钮为中文（日志无"未找到翻译"告警） |
| SQLite | 正常：`QSqlDatabase` 驱动加载成功，缓存读写正常 |

---

## 四、注意事项

1. **旧构建目录（Qt5）已失效**：`build-review/`、`tests/build/` 里的 Makefile 指向已卸载的 Qt5 路径，可以删除。
2. **不再为 Qt5 保留兼容代码**：源码里已无 `QT_VERSION < 6` 分支、无 `QTextCodec` / `QRegExp` / `QDesktopWidget` 等 Qt5 专属 API。
3. 若以后要发布安装包，记得带上：
   - Qt6 运行库（`Qt6Core/Gui/Widgets/Sql/Network.dll`）
   - 插件目录：`platforms/`、`sqldrivers/`（qsqlite）、`imageformats/`、`styles/`
   - `translations/qtbase_zh_CN.qm`
   - 程序自带的 `sqlite3.dll`
