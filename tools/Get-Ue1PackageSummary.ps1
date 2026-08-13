[CmdletBinding()]
param(
    [Parameter(Mandatory, ValueFromPipeline)]
    [string[]]$Path
)

process {
    foreach ($item in $Path) {
        $resolved = (Resolve-Path -LiteralPath $item).Path
        $stream = [IO.File]::OpenRead($resolved)
        $reader = [IO.BinaryReader]::new($stream)
        try {
            $signature = $reader.ReadUInt32()
            if ($signature -ne 2653586369) {
                throw "Not an Unreal package: $resolved"
            }

            $version = $reader.ReadUInt16()
            $licenseeMode = $reader.ReadUInt16()
            $flags = $reader.ReadUInt32()
            $nameCount = $reader.ReadUInt32()
            $nameOffset = $reader.ReadUInt32()
            $exportCount = $reader.ReadUInt32()
            $exportOffset = $reader.ReadUInt32()
            $importCount = $reader.ReadUInt32()
            $importOffset = $reader.ReadUInt32()

            $length = $stream.Length
            foreach ($offset in @($nameOffset, $exportOffset, $importOffset)) {
                if ($offset -gt $length) {
                    throw "Package table offset $offset exceeds file length $length in $resolved"
                }
            }

            [PSCustomObject]@{
                Path = $resolved
                Bytes = $length
                Version = $version
                LicenseeMode = $licenseeMode
                Flags = ('0x{0:X8}' -f $flags)
                Names = $nameCount
                Exports = $exportCount
                Imports = $importCount
            }
        } finally {
            $reader.Dispose()
            $stream.Dispose()
        }
    }
}
