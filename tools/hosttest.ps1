# Build and run host-side unit tests with zig's clang (tools/.venv).
# Usage: pwsh tools/hosttest.ps1 [test_name]
param([string]$Only = "")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$py = Join-Path $root "tools\.venv\Scripts\python.exe"
$out = Join-Path $root "host\build"
New-Item -ItemType Directory -Force $out | Out-Null

$core = @("components/story_core/story_arena.c", "components/story_core/story_mem.c")
$inc = @("-Icomponents/story_core/include", "-Icomponents/story_ui/include", "-Icomponents/story_llm/include", "-Ihost")

$tests = @{
    "test_arena" = @("host/test_arena.c") + $core
    "test_wrap"  = @("host/test_wrap.c", "components/story_ui/ui_wrap.c")
    "test_intent" = @("host/test_intent.c", "components/story_llm/story_intent.c")
    "llm_chat"   = @("host/llm_chat.c", "components/story_llm/neo.c", "components/story_llm/story_llm.c") + $core
}
$testArgs = @{
    "llm_chat" = @("models_out/llm3m/embed/model_neo_q4.bin", "models_out/llm3m/embed/tok_neo.bin")
}

Push-Location $root
try {
    foreach ($t in $tests.Keys) {
        if ($Only -and $t -ne $Only) { continue }
        $exe = Join-Path $out "$t.exe"
        & $py -m ziglang cc -O2 -g -DSTORY_HOST -Wall @inc @($tests[$t]) -o $exe -lm
        if ($LASTEXITCODE -ne 0) { throw "build failed: $t" }
        $a = $testArgs[$t]
        if ($a -and -not (Test-Path $a[0])) { Write-Host "SKIP $t (missing $($a[0]))"; continue }
        & $exe @a
        if ($LASTEXITCODE -ne 0) { throw "test failed: $t" }
    }
} finally { Pop-Location }
