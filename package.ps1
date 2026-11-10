<#
.SYNOPSIS
    一键打包 FileMarker 绿色版（windeployqt + 附带 DLL + 压缩包）。

.DESCRIPTION
    把编译好的 exe 放进一个独立目录，用 windeployqt 补齐 Qt 运行库，
    再拷上本工程自己的 sqlite3.dll 和 AI 打标脚本，最后压成 zip。

    产物：
      dist\FileMarker-portable\        可直接运行的目录
      dist\FileMarker-<yyyymmdd-HHmmss>.zip

.PARAMETER BuildDir
    构建目录（含 debug 或 release 子目录），相对本脚本所在目录，默认 ..\build-review。

.PARAMETER Config
    Debug 或 Release，默认 Release（打包一般用 release）。

.PARAMETER OutDir
    输出目录，默认 dist。

.PARAMETER QtDir
    Qt 的 mingw 套件目录（形如 ...\5.13.0\mingw73_64）。
    不传时依次尝试：环境变量 QTDIR → .pro 里 qmake 记录的路径 → 常见安装位置。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File package.ps1
    powershell -ExecutionPolicy Bypass -File package.ps1 -Config Debug -BuildDir build-review
    powershell -ExecutionPolicy Bypass -File package.ps1 -QtDir "D:\Qt\5.13.0\mingw73_64"
#>
param(
    [string] $BuildDir = "..\build-review",
    [ValidateSet("Debug", "Release")]
    [string] $Config = "Release",
    [string] $OutDir = "dist",
    [string] $QtDir = ""
)

$ErrorActionPreference = "Stop"

# ---- 路径 ----
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

# 找 Qt 套件目录：参数 > QTDIR 环境变量 > qmake 在 PATH 里 > 常见安装位置
function Resolve-QtDir {
    param([string] $Explicit, [string] $ProjectRoot)

    $candidates = New-Object System.Collections.Generic.List[string]
    if ($Explicit) { $candidates.Add($Explicit) }
    if ($env:QTDIR) { $candidates.Add($env:QTDIR) }

    # Qt Creator 会把套件路径记在 .pro.user 里，有就直接用
    $proUser = Join-Path $ProjectRoot "FileMarker.pro.user"
    if (Test-Path $proUser) {
        $match = Select-String -Path $proUser -Pattern 'mingw73_64' -List
        if ($match) {
            $found = [regex]::Match($match.Line, '[A-Za-z]:[\\/][^"'';]*mingw73_64')
            if ($found.Success) { $candidates.Add($found.Value) }
        }
    }

    $qmake = Get-Command qmake.exe -ErrorAction SilentlyContinue
    if ($qmake) { $candidates.Add((Split-Path -Parent $qmake.Source | Split-Path -Parent) + "\bin") }

    foreach ($root in @("E:\V_C\Qt", "C:\Qt", "D:\Qt")) {
        if (-not (Test-Path $root)) { continue }
        Get-ChildItem $root -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            $kit = Join-Path $_.FullName "mingw73_64"
            if (Test-Path $kit) { $candidates.Add($kit) }
            Get-ChildItem $_.FullName -Directory -ErrorAction SilentlyContinue | ForEach-Object {
                $inner = Join-Path $_.FullName "mingw73_64"
                if (Test-Path $inner) { $candidates.Add($inner) }
            }
        }
    }

    foreach ($c in $candidates) {
        if (-not $c) { continue }
        if (Test-Path (Join-Path $c "bin\windeployqt.exe")) { return $c }
    }
    return ""
}

if (-not $QtDir) { $QtDir = Resolve-QtDir -Explicit "" -ProjectRoot $ProjectRoot }
if (-not $QtDir -or -not (Test-Path (Join-Path $QtDir "bin\windeployqt.exe"))) {
    throw ("找不到 Qt 套件目录。请用 -QtDir 指定，例如：" + [Environment]::NewLine +
           '  powershell -File package.ps1 -QtDir "D:\Qt\5.13.0\mingw73_64"' + [Environment]::NewLine +
           "也可以设置环境变量 QTDIR 指向该目录。")
}

$QtBin = Join-Path $QtDir "bin"
$MinGwBin = Join-Path (Split-Path -Parent (Split-Path -Parent $QtDir)) "Tools\mingw730_64\bin"
$WindepLoy = Join-Path $QtBin "windeployqt.exe"
Write-Host "==> 使用 Qt：$QtDir" -ForegroundColor Cyan

$subDir = if ($Config -eq "Debug") { "debug" } else { "release" }
$exePath = Join-Path (Join-Path $ProjectRoot $BuildDir) (Join-Path $subDir "FileMarker.exe")
if (-not (Test-Path $exePath)) {
    throw "找不到可执行文件：$exePath（请先构建 $Config 版本）"
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$stageName = "FileMarker-portable"
$distRoot = Join-Path $ProjectRoot $OutDir
$stageDir = Join-Path $distRoot $stageName
$zipPath = Join-Path $distRoot ("FileMarker-{0}.zip" -f $stamp)

Write-Host "==> 清理旧的打包目录" -ForegroundColor Cyan
if (Test-Path $stageDir) { Remove-Item $stageDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

Write-Host "==> 拷贝可执行文件" -ForegroundColor Cyan
Copy-Item $exePath $stageDir

Write-Host "==> windeployqt 补齐 Qt 运行库" -ForegroundColor Cyan
$env:PATH = "$QtBin;$MinGwBin;$env:PATH"
$deployArgs = @(
    "--no-system-d3d-compiler",
    "--no-opengl-sw",
    "--compiler-runtime",
    "`"$($stageDir)\FileMarker.exe`""
)
if ($Config -eq "Debug") { $deployArgs = @("--debug") + $deployArgs }
$out = & $WindepLoy @deployArgs 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host $out
    throw "windeployqt 失败（exit=$LASTEXITCODE）"
}

Write-Host "==> 拷贝工程自带的运行时文件" -ForegroundColor Cyan
# sqlite3.dll 是本工程自己链接的，windeployqt 不认识
$libDir = Join-Path $ProjectRoot "lib"
if (Test-Path $libDir) {
    Get-ChildItem $libDir -Filter *.dll | ForEach-Object {
        Copy-Item $_.FullName $stageDir
        Write-Host ("    + {0}" -f $_.Name)
    }
}
# Qt 自带控件的中文翻译（文件对话框、消息框的按钮等）。
# windeployqt 会把所有语言的 .qm 都拷过来（几十种、多出 5 MB），这里只留中文。
$translationsDir = Join-Path $stageDir "translations"
if (Test-Path $translationsDir) {
    Get-ChildItem $translationsDir -Filter "qt_*.qm" |
        Where-Object { $_.Name -notin @("qt_zh_CN.qm", "qt_zh_TW.qm") } |
        Remove-Item -Force
}
$qtTranslationsSrc = Join-Path (Split-Path -Parent $QtBin) "translations"
foreach ($qm in @("qt_zh_CN.qm", "qt_zh_TW.qm")) {
    $src = Join-Path $qtTranslationsSrc $qm
    if (Test-Path $src) {
        New-Item -ItemType Directory -Force -Path $translationsDir | Out-Null
        Copy-Item $src $translationsDir -Force
        Write-Host ("    + translations/{0}" -f $qm)
    }
}

# AI 打标脚本：程序和脚本在同一目录才能找到
$pyScript = Join-Path $ProjectRoot "extract_and_tag.py"
if (Test-Path $pyScript) {
    Copy-Item $pyScript $stageDir
    Write-Host "    + extract_and_tag.py"
}
# 首次运行需要的说明
$readme = Join-Path $stageDir "使用说明.txt"
@"
FileMarker 绿色版（打包于 $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")）

1. 直接双击 FileMarker.exe 运行，不需要安装。
2. 标签保存在文件的 NTFS 数据流里：文件拷到 U 盘(FAT32)、压缩包、网盘时会丢失，
   建议先在程序里【文件 → 导出标签…】备份。
3. AI 自动打标需要本机有 python，并安装 extract_and_tag.py 依赖的库；
   脚本就在本目录，可在【帮助 → 检查 AI 打标环境…】里自检。
4. 标签库存在同目录的 labels.xml，偏好设置在同目录的 FileMarker.ini。
"@ | Set-Content -Path $readme -Encoding UTF8

Write-Host "==> 压缩" -ForegroundColor Cyan
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path $stageDir -DestinationPath $zipPath -CompressionLevel Optimal

$dirSizeMb = [math]::Round(((Get-ChildItem $stageDir -Recurse -File | Measure-Object Length -Sum).Sum / 1MB), 1)
$zipSizeMb = [math]::Round(((Get-Item $zipPath).Length / 1MB), 1)

Write-Host ""
Write-Host "打包完成：" -ForegroundColor Green
Write-Host ("  目录：{0}  ({1} MB)" -f $stageDir, $dirSizeMb)
Write-Host ("  压缩：{0}  ({1} MB)" -f $zipPath, $zipSizeMb)
