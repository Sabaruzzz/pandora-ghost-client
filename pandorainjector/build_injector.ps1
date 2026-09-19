param(
    [string]$inFile = "x64\Release\pandora.dll",
    [string]$outFile = "pandorainjector\payload.h"
)

if (-Not (Test-Path $inFile)) {
    Write-Host "[!] Error: No se encontro $inFile. Compila el cheat en Release x64 primero." -ForegroundColor Red
    exit
}

if (-Not (Test-Path "pandorainjector")) {
    New-Item -ItemType Directory -Force -Path "pandorainjector" | Out-Null
}

Write-Host "[*] Leyendo DLL..." -ForegroundColor Yellow
$bytes = [System.IO.File]::ReadAllBytes($inFile)

$stream = [System.IO.StreamWriter]::new($outFile)
$stream.WriteLine("#pragma once")
$stream.WriteLine("const unsigned char payload[] = {")

$line = ""
for ($i = 0; $i -lt $bytes.Length; $i++) {
    $encryptedByte = $bytes[$i] -bxor 0x93
    $line += "0x$($encryptedByte.ToString('X2')),"
    if (($i + 1) % 16 -eq 0) {
        $stream.WriteLine($line)
        $line = ""
    }
}
if ($line.Length -gt 0) { $stream.WriteLine($line) }

$stream.WriteLine("};")
$stream.WriteLine("const unsigned int payload_size = $($bytes.Length);")
$stream.Close()

Write-Host "[+] Exito: payload.h generado correctamente! Tamaño: $($bytes.Length) bytes." -ForegroundColor Green
