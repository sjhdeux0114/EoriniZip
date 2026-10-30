param([string]$Root = (Split-Path -Parent $PSScriptRoot))
Add-Type -AssemblyName System.Drawing
$source = [Drawing.Image]::FromFile((Join-Path $Root 'assets/logo.png'))
function Frame([int]$size) {
  $bitmap = [Drawing.Bitmap]::new($size, $size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $graphics = [Drawing.Graphics]::FromImage($bitmap)
  $graphics.Clear([Drawing.Color]::Transparent)
  $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $graphics.DrawImage($source, 0, 0, $size, $size)
  $stream = [IO.MemoryStream]::new()
  $bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
  $bytes = $stream.ToArray()
  $graphics.Dispose(); $bitmap.Dispose(); $stream.Dispose()
  return ,$bytes
}
$frames = @(16,24,32,48,64,128,256 | ForEach-Object { @{ Size=$_; Data=(Frame $_) } })
$stream = [IO.File]::Create((Join-Path $Root 'assets/EoriniZip.ico'))
$writer = [IO.BinaryWriter]::new($stream)
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$frames.Count)
$offset = 6 + 16 * $frames.Count
foreach ($frame in $frames) {
  $dim = if ($frame.Size -eq 256) { 0 } else { $frame.Size }
  $writer.Write([byte]$dim); $writer.Write([byte]$dim); $writer.Write([byte]0); $writer.Write([byte]0)
  $writer.Write([uint16]1); $writer.Write([uint16]32)
  $writer.Write([uint32]$frame.Data.Length); $writer.Write([uint32]$offset)
  $offset += $frame.Data.Length
}
foreach ($frame in $frames) { $writer.Write([byte[]]$frame.Data) }
$writer.Dispose()
$types = @(@('icp4',16),@('icp5',32),@('icp6',64),@('ic07',128),@('ic08',256),@('ic09',512),@('ic10',1024))
$frames = @($types | ForEach-Object { @{ Type=$_[0]; Data=(Frame $_[1]) } })
$stream = [IO.File]::Create((Join-Path $Root 'assets/EoriniZip.icns'))
$writer = [IO.BinaryWriter]::new($stream)
function BigEndian([uint32]$number) {
  $data = [BitConverter]::GetBytes($number); [Array]::Reverse($data); $writer.Write($data)
}
$writer.Write([Text.Encoding]::ASCII.GetBytes('icns'))
$total = 8; foreach ($frame in $frames) { $total += 8 + $frame.Data.Length }; BigEndian $total
foreach ($frame in $frames) {
  $writer.Write([Text.Encoding]::ASCII.GetBytes($frame.Type)); BigEndian (8 + $frame.Data.Length); $writer.Write([byte[]]$frame.Data)
}
$writer.Dispose(); $source.Dispose()
