$ErrorActionPreference = "Stop"

$source = "main.cpp"
$exe = "llm_overlay.exe"
function Compile-Msvc($clPath) {
  & $clPath /nologo /O1 /Os /GL /Gy /Gw /GF /MT /DNOMINMAX /DUNICODE /D_UNICODE /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 $source /Fe:$exe /link /LTCG /OPT:REF /OPT:ICF winhttp.lib user32.lib gdi32.lib advapi32.lib shell32.lib gdiplus.lib crypt32.lib
}

function Find-Tool($name) {
  $cmd = Get-Command $name -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Path }
  return $null
}

$cl = Find-Tool "cl.exe"
if ($cl) {
  Compile-Msvc $cl
  exit 0
}

$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
  $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
  if ($installPath) {
    $vcvars = Join-Path $installPath "VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path $vcvars) {
      $cmd = "`"$vcvars`" && cl /nologo /O1 /Os /GL /Gy /Gw /GF /MT /DNOMINMAX /DUNICODE /D_UNICODE /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 $source /Fe:$exe /link /LTCG /OPT:REF /OPT:ICF winhttp.lib user32.lib gdi32.lib advapi32.lib shell32.lib gdiplus.lib crypt32.lib"
      & cmd.exe /c $cmd
      if ($LASTEXITCODE -eq 0) { exit 0 }
    }
  }
}

$fallbacks = @(
  "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat",
  "C:\Program Files\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
  "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
  "C:\Program Files\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvars64.bat",
  "C:\Program Files\Microsoft Visual Studio\17\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
  "C:\Program Files (x86)\Microsoft Visual Studio\17\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)
foreach ($vcvars in $fallbacks) {
  if (Test-Path $vcvars) {
    $cmd = "`"$vcvars`" && cl /nologo /O1 /Os /GL /Gy /Gw /GF /MT /DNOMINMAX /DUNICODE /D_UNICODE /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 $source /Fe:$exe /link /LTCG /OPT:REF /OPT:ICF winhttp.lib user32.lib gdi32.lib advapi32.lib shell32.lib gdiplus.lib crypt32.lib"
    & cmd.exe /c $cmd
    if ($LASTEXITCODE -eq 0) { exit 0 }
  }
}

$gxx = Find-Tool "g++.exe"
if ($gxx) {
  & $gxx -Os -s -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 $source -o $exe -lwinhttp -lgdi32 -luser32 -ladvapi32 -lshell32 -lgdiplus -lcrypt32
  exit 0
}

Write-Error "No C compiler found. Install MSVC Build Tools or MinGW."
