# Build and run host-side unit tests with zig's clang (tools/.venv).
# Usage: pwsh tools/hosttest.ps1 [test_name]
param([string]$Only = "")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$py = Join-Path $root "tools\.venv\Scripts\python.exe"
$out = Join-Path $root "host\build"
New-Item -ItemType Directory -Force $out | Out-Null

$core = @("components/story_core/story_arena.c", "components/story_core/story_mem.c")
$inc = @("-Icomponents/story_core/include", "-Icomponents/story_ui/include", "-Ihost")

$tests = @{
    "test_arena" = @("host/test_arena.c") + $core
    "test_wrap"  = @("host/test_wrap.c", "components/story_ui/ui_wrap.c")
}

Push-Location $root
try {
    foreach ($t in $tests.Keys) {
        if ($Only -and $t -ne $Only) { continue }
        $exe = Join-Path $out "$t.exe"
        & $py -m ziglang cc -O2 -g -DSTORY_HOST -Wall @inc @($tests[$t]) -o $exe -lm
        if ($LASTEXITCODE -ne 0) { throw "build failed: $t" }
        & $exe
        if ($LASTEXITCODE -ne 0) { throw "test failed: $t" }
    }
} finally { Pop-Location }
