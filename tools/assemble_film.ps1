# Joins the film into one MP4 from the Blender frames (out\blender3d\f_NNNN.png).
# The numbers with no Blender frame are the gust transitions. -Transition decides what happens there:
#
#   Fade  (default)  crossfade the last Blender frame before the gap into the first one after it,
#                    over the same number of frames, so the film keeps its length and stays 3D
#   Cut              remove the gap: hard cut from one scene straight into the next (film gets shorter)
#   Gust             the original behaviour: use the C++ images (out\frames\f_NNNN.ppm) in the gap
#                    (those images are 2D, so the film jumps from 3D to 2D around every gust)
#
# Run from the repo root:   .\tools\assemble_film.ps1
# Examples:                 .\tools\assemble_film.ps1 -Transition Cut
#                           .\tools\assemble_film.ps1 -Png out\blender -Fps 24 -Out out\film.mp4
# Frames before the first Blender frame or after the last one are not used in Fade and Cut.

param(
    [ValidateSet("Fade", "Cut", "Gust")][string]$Transition = "Fade",
    [int]$Fps = 24,
    [string]$Png = "out\blender3d",     # folder with the Blender PNGs (f_NNNN.png)
    [string]$Ppm = "out\frames",        # folder with the C++ images (f_NNNN.ppm); only used by -Transition Gust
    [string]$Out = "out\film.mp4",
    [int]$Width = 640,
    [int]$Height = 360,
    [switch]$KeepTemp                   # keep the temporary frame folder for inspection
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot          # this file lives in <repo>\tools\

function Full([string]$p) {
    if ([IO.Path]::IsPathRooted($p)) { return $p }
    return (Join-Path $root $p)
}

if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    throw "ffmpeg not found. Install it with:  winget install Gyan.FFmpeg   (then close and reopen VS Code)"
}

$pngDir = Full $Png
$outFile = Full $Out
if (-not (Test-Path $pngDir)) { throw "Blender PNG folder not found: $pngDir  (use -Png to point at the folder with f_NNNN.png)" }

# ---- the Blender frames: number -> file ----
$have = @{}
foreach ($f in Get-ChildItem $pngDir -Filter "f_*.png") {
    if ($f.BaseName -match '^f_(\d+)$') { $have[[int]$Matches[1]] = $f }
}
if ($have.Count -eq 0) { throw "No f_NNNN.png files in $pngDir" }
$nums = @($have.Keys | Sort-Object)
$digits = ($have[$nums[0]].BaseName -replace '^f_', '').Length
Write-Host "Blender frames: $($nums.Count), numbers $($nums[0])-$($nums[$nums.Count - 1])"

# ---- temp folder outside the repo (OneDrive would try to sync thousands of files) ----
$tmp = Join-Path $env:TEMP "bench_film_frames"
if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
New-Item -ItemType Directory -Path $tmp | Out-Null

function Tmp-Name([int]$n) { return (Join-Path $tmp ("f_{0:D$digits}.png" -f $n)) }

if ($Transition -eq "Cut") {
    # renumber 0..N-1 so the gaps simply disappear
    $i = 0
    foreach ($n in $nums) {
        Copy-Item $have[$n].FullName (Join-Path $tmp ("s_{0:D6}.png" -f $i))
        $i++
    }
    $pattern = "s_%06d"
    $start = 0
    $total = $nums.Count
    Write-Host "Cut: $total frames, transitions removed"
}
elseif ($Transition -eq "Fade") {
    $first = $nums[0]
    $last = $nums[$nums.Count - 1]
    foreach ($n in $nums) { Copy-Item $have[$n].FullName (Tmp-Name $n) }
    $filled = 0
    $gaps = 0
    for ($i = 0; $i -lt $nums.Count - 1; $i++) {
        $a = $nums[$i]
        $b = $nums[$i + 1]
        $k = $b - $a - 1                       # frames missing between two Blender frames
        if ($k -le 0) { continue }
        $gaps++
        Write-Host "Crossfade over numbers $($a + 1)-$($b - 1) ($k frames) ..."
        for ($j = 1; $j -le $k; $j++) {
            $w = $j / ($k + 1)
            $wt = [string]::Format([System.Globalization.CultureInfo]::InvariantCulture, "{0:0.######}", $w)
            & ffmpeg -y -nostdin -loglevel error -i $have[$a].FullName -i $have[$b].FullName `
                -filter_complex "[0:v][1:v]blend=all_expr='A*(1-$wt)+B*$wt'" -frames:v 1 (Tmp-Name ($a + $j))
            if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed while making crossfade frame $($a + $j)" }
            $filled++
        }
    }
    $pattern = "f_%0${digits}d"
    $start = $first
    $total = $last - $first + 1
    Write-Host "Fade: $total frames ($($nums.Count) from Blender, $filled crossfade frames in $gaps gaps)"
}
else {
    # Gust: fill every missing number from the C++ image
    $ppmDir = Full $Ppm
    if (-not (Test-Path $ppmDir)) { throw "C++ frame folder not found: $ppmDir  (run ./bench --frames out/frames/f 24 --json first)" }
    $ppmFiles = @(Get-ChildItem $ppmDir -Filter "f_*.ppm" | Where-Object { $_.BaseName -match '^f_\d+$' })
    if ($ppmFiles.Count -eq 0) { throw "No f_NNNN.ppm files in $ppmDir" }
    $pn = @($ppmFiles | ForEach-Object { [int]($_.BaseName -replace '^f_', '') } | Sort-Object)
    $first = $pn[0]
    $last = $pn[$pn.Count - 1]
    $total = $last - $first + 1
    if ($pn.Count -ne $total) { throw "The C++ images have gaps. Regenerate them with ./bench --frames out/frames/f 24 --json" }
    $pattern = "f_%0${digits}d"
    $start = $first
    Write-Host "Converting C++ images ($total frames) ..."
    & ffmpeg -y -nostdin -loglevel error -start_number $first -i (Join-Path $ppmDir "${pattern}.ppm") -start_number $first (Join-Path $tmp "${pattern}.png")
    if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed while converting the C++ images" }
    $fromBlender = 0
    foreach ($n in $nums) {
        if ($n -ge $first -and $n -le $last) {
            Copy-Item $have[$n].FullName (Tmp-Name $n) -Force
            $fromBlender++
        }
    }
    Write-Host "Gust: $total frames ($fromBlender from Blender, $($total - $fromBlender) from C++ images)"
}

# ---- encode (scale makes sure every frame has the same size) ----
New-Item -ItemType Directory -Force -Path (Split-Path $outFile) | Out-Null
Write-Host "Encoding $outFile ..."
& ffmpeg -y -nostdin -loglevel error -stats -framerate $Fps -start_number $start -i (Join-Path $tmp "${pattern}.png") `
    -vf "scale=${Width}:${Height}:flags=lanczos,format=yuv420p" -c:v libx264 -crf 18 -preset medium -movflags +faststart $outFile
if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed while encoding" }

if (-not $KeepTemp) { Remove-Item $tmp -Recurse -Force }

$secs = [math]::Round($total / $Fps, 1)
$mb = [math]::Round((Get-Item $outFile).Length / 1MB, 1)
Write-Host ""
Write-Host "Done: $outFile  ($total frames, $secs s at $Fps fps, $mb MB, transition: $Transition)"