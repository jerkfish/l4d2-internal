# Install the MSVC v14.38.33130 (VS 2022 17.8) x86/x64 toolset into
# VS 2022 Community. Requires an elevated process; Start-Process -Verb RunAs
# raises the UAC prompt.
$ErrorActionPreference = 'Stop'

$installer = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe'
$installPath = 'C:\Program Files\Microsoft Visual Studio\2022\Community'
$component = 'Microsoft.VisualStudio.Component.VC.14.38.17.8.x86.x64'

# Start-Process joins an array without adding quotes, so build one command
# line with the install path explicitly quoted.
$argumentLine = 'modify --installPath "{0}" --add {1} --quiet --norestart' -f `
    $installPath, $component

$process = Start-Process -FilePath $installer -ArgumentList $argumentLine `
    -Verb RunAs -PassThru -Wait
Write-Host "VS installer exit code: $($process.ExitCode)"
exit $process.ExitCode
