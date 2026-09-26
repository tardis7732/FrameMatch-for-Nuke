param([string]$NukeExe=$env:NUKE_EXE,[switch]$Demo)
$ErrorActionPreference='Stop'
if (-not $NukeExe) {
    $candidates=@(Get-ChildItem -LiteralPath $env:ProgramFiles -Directory -Filter 'Nuke*' |
        ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -File -Filter 'Nuke*.exe'} |
        Where-Object {$_.Name -match '^Nuke\d.*\.exe$'} | Sort-Object LastWriteTime -Descending)
    if(-not $candidates.Count){throw 'Nuke was not found. Set NUKE_EXE to your Nuke executable path.'}
    $NukeExe=$candidates[0].FullName
}
if(-not(Test-Path -LiteralPath $NukeExe -PathType Leaf)){throw "Nuke executable not found: $NukeExe"}
$package=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$env:NUKE_PATH=(Join-Path $package 'nuke')+';'+$env:NUKE_PATH
$env:OFX_PLUGIN_PATH=(Join-Path $package 'ofx')+';'+$env:OFX_PLUGIN_PATH
$arguments='--nukex'
if($Demo){
    foreach($media in @('source.mp4','change_prores.mov')){
        if(-not(Test-Path -LiteralPath (Join-Path $package ('examples\media\'+$media)))){
            throw 'Download FrameMatch-Sample-Media.zip from Releases and extract it into this package folder first.'
        }
    }
    $arguments+=' "'+(Join-Path $package 'examples\FrameMatch_demo.nk')+'"'
}
Start-Process -FilePath $NukeExe -ArgumentList $arguments -WorkingDirectory $package
