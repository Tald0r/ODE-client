# Package the Release executable after the windows CI preset passes.
#
# The layout is the one the hand-built 0.0.7 release used: DarkEden.exe
# with every vcpkg DLL it loads, the English UI text overlay under Data/,
# and a license notice for each bundled library. Only the DLLs in the
# executable's dependency closure are copied - bin/Release also holds the
# runtime libraries of the tests and tools - and each one is checked
# against the whitelist in Client.cpp, which makes the client exit with -1
# before logging starts if it finds an unlisted DLL beside it.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Set-Location (Join-Path $PSScriptRoot '../..')

$binDir = 'build/presets/windows/bin/Release'
$binary = Join-Path $binDir 'DarkEden.exe'
$installed = 'build/presets/vcpkg_installed'
$name = 'darkeden-client-windows-x64'
$package = "build/packages/$name"
if (-not (Test-Path $binary)) { throw "Missing $binary; run the windows-release build first" }
Remove-Item -Recurse -Force $package, "$package.zip" -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $package | Out-Null

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$dumpbin = & $vswhere -latest -products * -find 'VC\Tools\MSVC\**\bin\Hostx64\x64\dumpbin.exe' |
	Select-Object -First 1
if (-not $dumpbin) { throw 'dumpbin.exe not found' }

function Get-Dependents([string]$path) {
	$lines = & $dumpbin /nologo /dependents $path
	if ($LASTEXITCODE -ne 0) { throw "dumpbin failed on $path" }
	$lines | ForEach-Object { $_.Trim() } | Where-Object { $_ -match '^[\w.+-]+\.dll$' }
}

# Walk the dependency closure; a DLL found in bin/Release is bundled, any
# other is the system's or the Visual C++ runtime's.
$bundled = [System.Collections.Generic.List[string]]::new()
$dependencies = [ordered]@{}
$queue = [System.Collections.Generic.Queue[string]]::new()
$queue.Enqueue('DarkEden.exe')
while ($queue.Count -gt 0) {
	$image = $queue.Dequeue()
	if ($dependencies.Contains($image)) { continue }
	$dependents = @(Get-Dependents (Join-Path $binDir $image))
	$dependencies[$image] = $dependents
	foreach ($dll in $dependents) {
		$local = Get-ChildItem -LiteralPath $binDir -Filter $dll -File -ErrorAction SilentlyContinue
		if ($local -and -not ($bundled -contains $local.Name)) {
			$bundled.Add($local.Name)
			$queue.Enqueue($local.Name)
		}
	}
}
if ($bundled.Count -eq 0) { throw 'DarkEden.exe loads no DLL from bin/Release; the deployment is missing' }

$allowed = Select-String -Path 'Client/Client.cpp' -Pattern 'InvalidDll != "([^"]+\.dll)"' -AllMatches |
	ForEach-Object { $_.Matches } | ForEach-Object { $_.Groups[1].Value.ToLowerInvariant() }
foreach ($dll in $bundled) {
	if (-not ($allowed -contains $dll.ToLowerInvariant())) {
		throw "$dll is not in the DLL whitelist in Client/Client.cpp; the client would exit at startup"
	}
}

Copy-Item $binary $package
foreach ($dll in $bundled) { Copy-Item (Join-Path $binDir $dll) $package }
Copy-Item -Recurse 'tools/i18n/ui-text/Data' (Join-Path $package 'Data')

# One notice per vcpkg port that installed a bundled DLL, found through the
# port's file list, plus the libraries compiled into the executable.
$licenses = Join-Path $package 'licenses'
New-Item -ItemType Directory -Force -Path (Join-Path $licenses 'unrar') | Out-Null
$ports = [System.Collections.Generic.SortedSet[string]]::new()
foreach ($dll in $bundled) {
	$owner = Get-ChildItem "$installed/vcpkg/info/*.list" |
		Where-Object { Select-String -LiteralPath $_.FullName -Pattern "/bin/$([regex]::Escape($dll))$" -Quiet } |
		Select-Object -First 1
	if (-not $owner) { throw "No vcpkg port installed $dll" }
	[void]$ports.Add($owner.Name.Split('_')[0])
}
foreach ($port in $ports) {
	Copy-Item "$installed/x64-windows/share/$port/copyright" (Join-Path $licenses "$port.txt")
}
Copy-Item 'build/presets/windows/_deps/ixwebsocket-src/LICENSE.txt' (Join-Path $licenses 'ixwebsocket.txt')
Copy-Item 'third_party/xbrz/License.txt' (Join-Path $licenses 'xbrz.txt')
Copy-Item 'third_party/unrar/license.txt', 'third_party/unrar/acknow.txt', 'third_party/unrar/README.md' `
	(Join-Path $licenses 'unrar')

$commit = git rev-parse HEAD
@"
DarkEden client - Windows x64 CI build

Built and tested on GitHub Actions (windows-2022) using the windows-release
preset. Use 64-bit Windows 10 or later.

Extract this ZIP into your game directory. Download and extract runtime assets v2
into the same directory, so Data/ is beside DarkEden.exe:
https://github.com/bound2/opendarkeden-client/releases/tag/assets-v2

Install the Microsoft Visual C++ Redistributable (x64) if needed:
https://aka.ms/vc14/vc_redist.x64.exe
Run DarkEden.exe from the game directory. Existing UserSet/ settings can be retained.

Data/ holds the English text of the packed UI resources (item, skill, help,
book and tutorial text); it merges with the assets, which stay Korean inside
their archives, and the client prefers these loose files.
SDL/codec runtime DLLs and dependency license notices are included.
The OpenSSL DLLs (libssl-3-x64.dll, libcrypto-3-x64.dll) are for the optional
native WebSocket transport (DARKEDEN_WEBSOCKET_URL, see docs/webgl-client.md).
Game assets and account settings are not included.

BUILD-INFO.txt records the exact source revision and build environment.
DEPENDENCIES.txt records what each bundled image links.
"@ | Set-Content -Encoding utf8 (Join-Path $package 'README.txt')

$compiler = Select-String -Path 'build/presets/windows/CMakeFiles/*/CMakeCXXCompiler.cmake' `
	-Pattern 'set\(CMAKE_CXX_COMPILER_VERSION "([^"]+)"\)' | Select-Object -First 1
$portVersions = Get-ChildItem "$installed/vcpkg/info/*.list" | ForEach-Object { $_.BaseName }
@(
	'Source: https://github.com/bound2/opendarkeden-client'
	"Commit: $commit"
	'Preset: windows-release (Release)'
	"CI: https://github.com/$env:GITHUB_REPOSITORY/actions/runs/$env:GITHUB_RUN_ID"
	"Built: $((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))"
	"Runner: $([System.Environment]::OSVersion.VersionString)"
	"MSVC: $($compiler.Matches[0].Groups[1].Value)"
	'vcpkg ports:'
	($portVersions | ForEach-Object { "  $_" })
) | Set-Content -Encoding utf8 (Join-Path $package 'BUILD-INFO.txt')
$dependencies.GetEnumerator() | ForEach-Object {
	"$($_.Key):"
	$_.Value | ForEach-Object { "  $_" }
} | Set-Content -Encoding utf8 (Join-Path $package 'DEPENDENCIES.txt')

Compress-Archive -Path "$package/*" -DestinationPath "$package.zip"
Get-ChildItem -Recurse -File $package | ForEach-Object { $_.FullName.Substring((Resolve-Path $package).Path.Length + 1) } |
	Where-Object { $_ -notlike 'Data*' }
Get-FileHash -Algorithm SHA256 "$package.zip" | Format-List
