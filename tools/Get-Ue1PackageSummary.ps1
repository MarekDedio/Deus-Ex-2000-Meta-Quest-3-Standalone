[CmdletBinding()]
param(
    [Parameter(Mandatory, ValueFromPipeline)]
    [string[]]$Path
)

begin {
    function Read-CompactIndex {
        param([Parameter(Mandatory)][IO.BinaryReader]$Reader)

        $current = $Reader.ReadByte()
        $negative = ($current -band 0x80) -ne 0
        [uint32]$magnitude = $current -band 0x3f
        $more = ($current -band 0x40) -ne 0
        $shift = 6
        while ($more -and $shift -lt 32) {
            $current = $Reader.ReadByte()
            $magnitude = $magnitude -bor ([uint32]($current -band 0x7f) -shl $shift)
            $more = ($current -band 0x80) -ne 0
            $shift += 7
        }
        if ($more -or $magnitude -gt [int]::MaxValue) {
            throw 'Invalid compact index'
        }
        if ($negative) { return -[int]$magnitude }
        return [int]$magnitude
    }
}

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

            $stream.Position = $nameOffset
            $firstName = $null
            for ($nameIndex = 0; $nameIndex -lt $nameCount; $nameIndex++) {
                $serializedLength = Read-CompactIndex -Reader $reader
                if ($serializedLength -le 0 -or $serializedLength -gt 4096) {
                    throw "Invalid name length $serializedLength at index $nameIndex in $resolved"
                }
                $nameBytes = $reader.ReadBytes($serializedLength)
                if ($nameBytes.Count -ne $serializedLength -or $nameBytes[-1] -ne 0) {
                    throw "Invalid name entry at index $nameIndex in $resolved"
                }
                if ($nameIndex -eq 0) {
                    $firstName = [Text.Encoding]::ASCII.GetString(
                        $nameBytes, 0, $serializedLength - 1)
                }
                $null = $reader.ReadUInt32()
            }
            if ([string]::IsNullOrEmpty($firstName)) {
                throw "Empty first UE1 name in $resolved"
            }

            [PSCustomObject]@{
                Path = $resolved
                Bytes = $length
                Version = $version
                LicenseeMode = $licenseeMode
                Flags = ('0x{0:X8}' -f $flags)
                Names = $nameCount
                NamesParsed = $nameCount
                FirstName = $firstName
                Exports = $exportCount
                Imports = $importCount
            }
        } finally {
            $reader.Dispose()
            $stream.Dispose()
        }
    }
}
