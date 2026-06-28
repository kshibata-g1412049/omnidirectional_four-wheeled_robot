<#
.SYNOPSIS
  Build the Docker image and run the headless smoke test (Windows).

.DESCRIPTION
  PowerShell equivalent of scripts/docker_build_and_test.sh, for Windows
  hosts running Docker Desktop without WSL/Git Bash.

.PARAMETER ImageTag
  Docker image tag to build. Defaults to "omni4wd:<RosDistro>".

.PARAMETER RosDistro
  ROS 2 distro to build: humble | jazzy | rolling. Defaults to "jazzy"
  (or $env:ROS_DISTRO if set).

.EXAMPLE
  .\scripts\docker_build_and_test.ps1
.EXAMPLE
  .\scripts\docker_build_and_test.ps1 -ImageTag omni4wd:humble -RosDistro humble
#>
param(
    [string]$ImageTag = "",
    [string]$RosDistro = ""
)

$ErrorActionPreference = "Stop"

if (-not $RosDistro) {
    $RosDistro = if ($env:ROS_DISTRO) { $env:ROS_DISTRO } else { "jazzy" }
}
if (-not $ImageTag) {
    $ImageTag = "omni4wd:$RosDistro"
}

$RepoDir = Split-Path -Parent $PSScriptRoot
Set-Location $RepoDir

# --- proxy / CA wiring (optional) ------------------------------------------
$BuildArgs = @("--build-arg", "ROS_DISTRO=$RosDistro")
$NetArgs   = @()

$Proxy = if ($env:HTTPS_PROXY) { $env:HTTPS_PROXY } elseif ($env:https_proxy) { $env:https_proxy } else { $null }
if ($Proxy) {
    Write-Host "[build] using proxy: $Proxy (build with --network=host)"
    $NoProxy = if ($env:NO_PROXY) { $env:NO_PROXY } elseif ($env:no_proxy) { $env:no_proxy } else { "localhost,127.0.0.1" }
    $BuildArgs += @("--build-arg", "http_proxy=$Proxy",
                    "--build-arg", "https_proxy=$Proxy",
                    "--build-arg", "no_proxy=$NoProxy")
    $NetArgs += "--network=host"
}

# Stage a CA bundle into the build context if one was provided.
$CaSrc = if ($env:EXTRA_CA_BUNDLE) { $env:EXTRA_CA_BUNDLE } else { Join-Path $RepoDir "ca-bundle.crt" }
$CaDest = Join-Path $RepoDir "ca-bundle.crt"
$CleanCa = $false
if ((Test-Path $CaSrc) -and ($CaSrc -ne $CaDest) -and -not (Test-Path $CaDest)) {
    Write-Host "[build] staging CA bundle from $CaSrc"
    Copy-Item $CaSrc $CaDest
    $CleanCa = $true
}

try {
    # --- build ---------------------------------------------------------------
    Write-Host "[build] docker build -t $ImageTag (ROS_DISTRO=$RosDistro) ..."
    docker build @NetArgs @BuildArgs -t $ImageTag $RepoDir
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    # --- run smoke test --------------------------------------------------------
    Write-Host "[test] running smoke_test.sh inside the container ..."
    docker run --rm @NetArgs $ImageTag smoke_test.sh
    $Rc = $LASTEXITCODE

    if ($Rc -eq 0) {
        Write-Host "[test] RESULT: PASS ($RosDistro)"
    } else {
        Write-Host "[test] RESULT: FAIL ($RosDistro, rc=$Rc)"
    }
    exit $Rc
}
finally {
    if ($CleanCa) {
        Remove-Item $CaDest -ErrorAction SilentlyContinue
    }
}
