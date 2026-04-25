#Requires -RunAsAdministrator

$ServerFolder = Split-Path -Parent $MyInvocation.MyCommand.Path

# --- 1. Check / Install Python ------------------------------------------
Write-Host "Checking Python installation..." -ForegroundColor Cyan

$python = Get-Command python -ErrorAction SilentlyContinue

if (-not $python) {
    Write-Host "Python not found. Downloading and installing Python..." -ForegroundColor Yellow

    $installerUrl = "https://www.python.org/ftp/python/3.12.3/python-3.12.3-amd64.exe"
    $installerPath = "$env:TEMP\python-installer.exe"

    Invoke-WebRequest -Uri $installerUrl -OutFile $installerPath -UseBasicParsing

    # Install silently, add to PATH, install for all users
    Start-Process -FilePath $installerPath -ArgumentList "/quiet InstallAllUsers=1 PrependPath=1 Include_pip=1" -Wait

    Remove-Item $installerPath -Force

    # Refresh PATH in current session
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" +
                [System.Environment]::GetEnvironmentVariable("Path", "User")

    $python = Get-Command python -ErrorAction SilentlyContinue

    if (-not $python) {
        Write-Error "Python installation failed. Please install manually from https://www.python.org"
        exit 1
    }

    Write-Host "Python installed successfully: $(python --version)" -ForegroundColor Green
} else {
    Write-Host "Python found: $(python --version)" -ForegroundColor Green
}

# --- 2. Ensure pip is available -----------------------------------------
$pip = Get-Command pip -ErrorAction SilentlyContinue
if (-not $pip) {
    Write-Host "Bootstrapping pip..." -ForegroundColor Yellow
    python -m ensurepip --upgrade
}

# --- 3. Install requirements --------------------------------------------
$requirementsFile = Join-Path $ServerFolder "requirements.txt"

if (Test-Path $requirementsFile) {
    Write-Host "Installing requirements from requirements.txt..." -ForegroundColor Cyan
    python -m pip install --upgrade pip | Out-Null
    python -m pip install -r $requirementsFile

    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to install one or more requirements."
        exit 1
    }
    Write-Host "Requirements installed successfully." -ForegroundColor Green
} else {
    Write-Warning "requirements.txt not found at $requirementsFile - skipping."
}

# --- 4. Add server folder to PATH if not already present ----------------
Write-Host "Checking if server folder is in PATH..." -ForegroundColor Cyan

$machinePath = [System.Environment]::GetEnvironmentVariable("Path", "Machine")
$userPath    = [System.Environment]::GetEnvironmentVariable("Path", "User")
$combinedPath = ($machinePath + ";" + $userPath) -split ";" | ForEach-Object { $_.TrimEnd("\") }

if ($combinedPath -notcontains $ServerFolder.TrimEnd("\")) {
    Write-Host "Adding '$ServerFolder' to User PATH..." -ForegroundColor Yellow

    $newUserPath = ($userPath.TrimEnd(";") + ";" + $ServerFolder).TrimStart(";")
    [System.Environment]::SetEnvironmentVariable("Path", $newUserPath, "User")

    # Reflect change in current session
    $env:Path = $env:Path + ";" + $ServerFolder

    Write-Host "Folder added to PATH. Restart your shell for it to take effect in new sessions." -ForegroundColor Green
} else {
    Write-Host "Folder is already in PATH." -ForegroundColor Green
}

Write-Host ""
Write-Host "Setup complete." -ForegroundColor Cyan
