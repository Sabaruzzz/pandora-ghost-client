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

if (-not ("PandoraPayloadWriter" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Text;

public static class PandoraPayloadWriter {
    private const string Hex = "0123456789ABCDEF";
    public static void Write(string path, byte[] bytes) {
        using (var stream = new StreamWriter(path, false, Encoding.ASCII, 1 << 20)) {
            stream.WriteLine("#pragma once");
            stream.WriteLine("const unsigned char payload[] = {");
            char[] row = new char[81];
            int pos = 0;
            for (int i = 0; i < bytes.Length; ++i) {
                byte value = (byte)(bytes[i] ^ 0x93);
                row[pos++] = '0'; row[pos++] = 'x';
                row[pos++] = Hex[value >> 4]; row[pos++] = Hex[value & 15];
                row[pos++] = ',';
                if ((i + 1) % 16 == 0) {
                    stream.WriteLine(row, 0, pos);
                    pos = 0;
                }
            }
            if (pos > 0) stream.WriteLine(row, 0, pos);
            stream.WriteLine("};");
            stream.WriteLine("const unsigned int payload_size = " + bytes.Length + ";");
        }
    }
}
"@
}

$outFile = [System.IO.Path]::GetFullPath($outFile)
$outDirectory = [System.IO.Path]::GetDirectoryName($outFile)
[System.IO.Directory]::CreateDirectory($outDirectory) | Out-Null

# MSBuild can launch this target more than once when the DLL and injector are
# built together. Serialize generators and publish only a complete header.
$mutex = [System.Threading.Mutex]::new($false, "Local\PandoraClientPayloadGenerator")
$lockTaken = $false
$tempFile = Join-Path $outDirectory ("payload.{0}.tmp" -f [System.Guid]::NewGuid().ToString("N"))
try {
    $lockTaken = $mutex.WaitOne([TimeSpan]::FromSeconds(45))
    if (-not $lockTaken) { throw "Timeout esperando el generador de payload." }

    [PandoraPayloadWriter]::Write($tempFile, $bytes)

    # Move-Item operates inside the same directory, so consumers see either
    # the previous complete header or the new complete header, never half a file.
    Move-Item -LiteralPath $tempFile -Destination $outFile -Force
}
finally {
    if (Test-Path -LiteralPath $tempFile) { Remove-Item -LiteralPath $tempFile -Force }
    if ($lockTaken) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
}

Write-Host "[+] Exito: payload.h generado correctamente! Tamaño: $($bytes.Length) bytes." -ForegroundColor Green
