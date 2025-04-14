Write-Host "Arguments : $args"
$bin_path = $args[0]
$file_version = $args[1]
$product_version = [String]::Join(".", $file_version.Split(".")[0..1])

# Extract version information from the relevant binaries
$oiio = Join-Path $bin_path "OpenImageIO.dll"
$oiio_util = Join-Path $bin_path "OpenImageIO_Util.dll"
$ver_oiio = (Get-ItemProperty $oiio).VersionInfo
$ver_oiio_util = (Get-ItemProperty $oiio_util).VersionInfo

# Stringify the version info for easy comparison
$expected = "OpenImageIO|$file_version|$product_version"
$actual_oiio = [String]::Join("|", $ver_oiio.FileDescription, $ver_oiio.FileVersion, $ver_oiio.ProductVersion)
$actual_oiio_util = [String]::Join("|", $ver_oiio_util.FileDescription, $ver_oiio_util.FileVersion, $ver_oiio_util.ProductVersion)

Write-Host "Expected : $expected"
Write-Host "Actual   : $actual_oiio"
Write-Host "         : $actual_oiio_util"

# Return 0 back to the shell if everything matches successfully
if (($actual_oiio -eq $expected) -and ($actual_oiio_util -eq $expected)) {
    exit 0 # success
}

exit -1