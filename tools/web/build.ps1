param([int]$Jobs = 8, [string]$Volume = 'darkeden-webgl-work')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output = Join-Path $repo 'build/web'
New-Item -ItemType Directory -Force $output | Out-Null
$image = 'emscripten/emsdk:6.0.10@sha256:e077d54e2b8970575ebc4f185ac1de0b95c05f2b266134d4ba27449af7aebf65'
docker run --rm --mount "type=bind,source=$repo,target=/src,readonly" `
    --mount "type=volume,source=$Volume,target=/work" `
    --mount 'type=volume,source=darkeden-emsdk-6-0-10-cache,target=/emsdk/upstream/emscripten/cache' `
    --env "WEB_BUILD_JOBS=$Jobs" $image bash /src/tools/web/build-container.sh
if ($LASTEXITCODE -ne 0) { throw 'Browser build failed' }
docker run --rm --mount "type=volume,source=$Volume,target=/work,readonly" `
    --mount "type=bind,source=$output,target=/out" $image bash -c `
    'cp /work/build-wasm/bin/DarkEden.mjs /work/build-wasm/bin/DarkEden.wasm /work/build-wasm/bin/index.html /work/build-wasm/bin/launcher.mjs /work/build-wasm/bin/touch-controls.mjs /work/build-wasm/bin/asset-store.mjs /work/build-wasm/bin/client-config.json /work/build-wasm/bin/web_sprite_tests.* /work/build-wasm/bin/transport_tests.* /work/build-wasm/bin/decore_tests.* /out/'
if ($LASTEXITCODE -ne 0) { throw 'Cannot copy browser build output' }
Write-Output "Browser files: $output"
