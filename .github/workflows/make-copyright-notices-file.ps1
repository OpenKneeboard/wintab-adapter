param (
    [Parameter(Mandatory=$true)]
    [string]$OutputFile,
    [Parameter(Mandatory=$true)]
    [string]$Arch,
    [Parameter(Mandatory=$true)]
    [string]$BuildDir
)

$vcpkgRoot = "$BuildDir/vcpkg_installed/$Arch-windows-static/share"

# Use an Ordered dictionary to keep wintab-adapter at the top
$licenses = [ordered]@{
    "wintab-adapter" = "LICENSE"
}

if (Test-Path $vcpkgRoot) {
    Get-ChildItem -Path $vcpkgRoot -Filter "copyright" -Recurse | ForEach-Object {
        $packageName = $_.Directory.Name
        $licenses[$packageName] = $_.FullName
    }
} else {
    Write-Warning "vcpkg share directory not found at: $vcpkgRoot"
}

$parentDir = Split-Path $OutputFile
if ($parentDir -and !(Test-Path $parentDir)) {
    New-Item -ItemType Directory -Path $parentDir -Force
}

$first = $true
foreach ($entry in $licenses.GetEnumerator()) {
    $header = $entry.Key
    $filePath = $entry.Value

    if (Test-Path $filePath) {
        $content = Get-Content $filePath -Raw

        $output = if ($first) { "" } else { "`n`n" }
        $output += "$header`n" + ("=" * $header.Length) + "`n`n"
        $output += $content

        if ($first) {
            $output | Out-File -FilePath $OutputFile -Encoding utf8
            $first = $false
        } else {
            $output | Out-File -FilePath $OutputFile -Append -Encoding utf8
        }
    } else {
        Write-Warning "License file not found for $header at $filePath"
    }
}