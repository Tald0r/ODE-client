param(
	[Parameter(Mandatory = $true)]
	[ValidateSet('windows', 'windows-asan')]
	[string]$ConfigurePreset,
	[Parameter(Mandatory = $true)]
	[ValidateSet('windows-debug', 'windows-release', 'windows-asan')]
	[string[]]$BuildPresets
)

$ErrorActionPreference = 'Stop'
function Invoke-LoggedCMake {
	param([string[]]$Arguments, [string]$LogPath)
	$cmakeExe = (Get-Command cmake.exe -ErrorAction Stop).Source
	# Windows PowerShell 5 turns redirected native stderr into error records.
	# A successful configure may emit warnings there; the exit code is authoritative.
	$ErrorActionPreference = 'Continue'
	& $cmakeExe @Arguments *> $LogPath
	if ($LASTEXITCODE -ne 0) {
		Get-Content $LogPath -Tail 150
		throw "CMake failed; see $LogPath"
	}
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $repoRoot
try {
	foreach ($preset in $BuildPresets) {
		if (($preset -eq 'windows-asan') -ne ($ConfigurePreset -eq 'windows-asan')) {
			throw "Build preset $preset does not use configure preset $ConfigurePreset"
		}
	}
	if (-not $env:VCPKG_ROOT -or -not (Test-Path "$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake")) {
		throw 'Set VCPKG_ROOT to a bootstrapped vcpkg checkout before running verification.'
	}

	# Explicit Git-for-Windows tools avoid accidentally selecting WSL's bash.
	$gitExe = (Get-Command git.exe -ErrorAction Stop).Source
	$baseline = (Get-Content vcpkg.json -Raw | ConvertFrom-Json).'builtin-baseline'
	$vcpkgRevision = & $gitExe -C $env:VCPKG_ROOT rev-parse HEAD
	if ($LASTEXITCODE -ne 0 -or $vcpkgRevision -ne $baseline) {
		throw "The verification script requires vcpkg revision $baseline; VCPKG_ROOT currently has $vcpkgRevision."
	}
	$gitAncestor = Split-Path $gitExe
	$bashExe = $null
	$perlExe = $null
	# git.exe can be exposed from cmd/, bin/, or mingw64/bin/.
	for ($level = 0; $level -lt 3; ++$level) {
		$gitTools = Join-Path $gitAncestor 'usr/bin'
		if ((Test-Path "$gitTools/bash.exe") -and (Test-Path "$gitTools/perl.exe")) {
			$bashExe = Join-Path $gitTools 'bash.exe'
			$perlExe = Join-Path $gitTools 'perl.exe'
			break
		}
		$gitAncestor = Split-Path $gitAncestor
	}
	if (-not $bashExe -or -not $perlExe) {
		throw "Git for Windows Bash and Perl are required; could not locate usr/bin relative to $gitExe"
	}
	$logDir = Join-Path $repoRoot "build/verification/$ConfigurePreset"
	New-Item -ItemType Directory -Force -Path $logDir | Out-Null
	Invoke-LoggedCMake -Arguments @('--preset', $ConfigurePreset,
		"-DBASH_EXECUTABLE=$bashExe", "-DPERL_EXECUTABLE=$perlExe") -LogPath "$logDir/configure.log"

	$requiredTests = @('unit_tests', 'user_option_tests', 'ui_tests', 'decore_tests', 'decore_vendored', 'ratchets', 'arch_includes',
		'source_encoding', 'warning_policy', 'warning_budget_parser', 'format_arity', 'packet_indices', 'wire_inventory_fresh')
	$warningFailures = @()
	foreach ($preset in $BuildPresets) {
		Write-Host "Building all targets: $preset (log: $logDir/$preset-build.log)"
		# A complete rebuild is required to compare the warning population.
		Invoke-LoggedCMake -Arguments @('--build', '--preset', $preset, '--clean-first') -LogPath "$logDir/$preset-build.log"
		$inventory = ctest --preset $preset --show-only=json-v1
		if ($LASTEXITCODE -ne 0) { throw "Cannot list tests: $preset" }
		$testNames = @(($inventory | ConvertFrom-Json).tests | ForEach-Object { $_.name })
		foreach ($required in $requiredTests) {
			if ($required -notin $testNames) { throw "Required test missing from ${preset}: $required" }
		}
		ctest --preset $preset --output-junit "$logDir/$preset-tests.xml"
		if ($LASTEXITCODE -ne 0) { throw "Tests failed: $preset" }
		try {
			# PowerShell 5 must not turn Perl's diagnostic stderr into an early
			# terminating error: collect every requested configuration first.
			$ErrorActionPreference = 'Continue'
			& $perlExe tools/ci/check-warnings.pl --log "$logDir/$preset-build.log" `
				--profile "$preset-x64" --baseline tools/ci/warning-baselines.json `
				--report "$logDir/$preset-warnings.json"
			$warningExit = $LASTEXITCODE
		}
		finally { $ErrorActionPreference = 'Stop' }
		if ($warningExit -ne 0) { $warningFailures += $preset }
	}
	if ($warningFailures.Count -gt 0) { throw "Warning budgets failed: $($warningFailures -join ', ')" }
}
finally {
	Pop-Location
}
