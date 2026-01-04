# Advanced Benchmark Script for OIIO Resample (Windows)

$RefOIIO = "e:\DVS\bin\oiiotool.exe"
$NewOIIO = "..\build\bin\Release\oiiotool.exe"

# Fallback check
if (!(Test-Path $NewOIIO)) {
    if (Test-Path "..\build\bin\Debug\oiiotool.exe") {
        $NewOIIO = "..\build\bin\Debug\oiiotool.exe"
    } else {
        $NewOIIO = "oiiotool.exe" # Assume PATH
    }
}

$Iterations = 10

Write-Host "Reference OIIO: $RefOIIO"
Write-Host "New OIIO:       $NewOIIO"
Write-Host "Iterations:     $Iterations"
Write-Host "------------------------------------------------"

# 1. Generate Images
Write-Host "Generating test images..."
if (!(Test-Path "bench_u8.tif")) {
    & $NewOIIO --pattern checker 2048x2048 3 -d uint8 -o bench_u8.tif | Out-Null
}
if (!(Test-Path "bench_u16.tif")) {
    & $NewOIIO --pattern checker 2048x2048 3 -d uint16 -o bench_u16.tif | Out-Null
}
if (!(Test-Path "bench_u32.tif")) {
    & $NewOIIO --pattern checker 2048x2048 3 -d uint32 -o bench_u32.tif | Out-Null
}
if (!(Test-Path "bench_f16.exr")) {
    & $NewOIIO --pattern checker 2048x2048 3 -d half -o bench_f16.exr | Out-Null
}
if (!(Test-Path "bench_f32.tif")) {
    & $NewOIIO --pattern checker 2048x2048 3 -d float -o bench_f32.tif | Out-Null
}
if (!(Test-Path "bench_f64.tif")) {
    & $NewOIIO --pattern checker 2048x2048 3 -d double -o bench_f64.tif | Out-Null
}

function Measure-AvgMs {
    param ($Tool, $InputFile, $Scale)
    
    # Warmup
    & $Tool $InputFile --resample:interp=1 $Scale -o out.nul | Out-Null

    $Total = 0
    for ($i=0; $i -lt $Iterations; $i++) {
        $Time = Measure-Command { 
            & $Tool $InputFile --resample:interp=1 $Scale -o out.nul | Out-Null 
        }
        $Total += $Time.TotalMilliseconds
    }
    return [math]::Round($Total / $Iterations, 2)
}

$Results = @()

$Images = @("bench_u8.tif", "bench_u16.tif", "bench_u32.tif", "bench_f32.tif", "bench_f64.tif", "bench_f16.exr")
$Scales = @("75%", "50%", "25%")

Write-Host "Starting Benchmark..."
Write-Host ""

foreach ($Img in $Images) {
    foreach ($Scale in $Scales) {
        Write-Host -NoNewline "Benchmarking $Img @ $Scale ... "
        $TimeRef = Measure-AvgMs $RefOIIO $Img $Scale
        $TimeNew = Measure-AvgMs $NewOIIO $Img $Scale
        
        $Speedup = if ($TimeNew -gt 0) { [math]::Round($TimeRef / $TimeNew, 2) } else { "Inf" }
        
        Write-Host "Done."
        
        $Results += [PSCustomObject]@{
            Image = $Img
            Scale = $Scale
            Ref_ms = $TimeRef
            New_ms = $TimeNew
            Speedup = "${Speedup}x"
        }
    }
}

Write-Host ""
Write-Host "RESULTS TABLE"
Write-Host "-------------"
$Results | Format-Table -AutoSize