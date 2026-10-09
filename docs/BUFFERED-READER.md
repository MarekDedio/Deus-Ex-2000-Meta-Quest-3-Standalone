# Bounded read-only table buffering

`native/portable_buffered_file.h` supplies a bounded File adapter selected only
by `LoadPortablePackageTables`. The raw File is private to the wrapper; callers
and PackageStream share its logical cursor. Other payload readers are unchanged.
Original-data integration of this selection passes on the host; no on-device
speedup is claimed. It leaves pinned vendor files unchanged.

A read-only diagnostic of the original actor integration found active work,
not a stalled process: about 26 million Windows read operations transferred
about 62 MB. The pinned Windows File reader issues a ReadFile for each scalar,
and repeated structural inspections rebuild source-package tables in fresh
hosts. These counters establish tiny-read churn, not an exact profile of every
hotspot. Android's pinned File uses stdio, so Windows results must not be treated
as Quest performance measurements.

## Adapter contract

The source must retain the pinned exact-read contract, immutable contents/size
and exclusive cursor ownership for the adapter's lifetime. It is not a snapshot,
mutation detector or concurrent-access wrapper. A production integration must
keep the raw source private and must not update game data while readers use it.

The cache defaults to 64 KiB and cannot exceed 1 MiB. Large spans bypass cache;
each direct source read is capped at 1 MiB, avoiding whole-package allocations
and the pinned Windows reader's oversized-call loop. Successful bytes and public
seek/tell positions are preserved. Offsets and spans are checked before source
access. All writes, including zero-sized writes, are rejected.

Predictable EOF spans reach the source directly, retaining partial destination
copy, actual cursor and exception behavior. Any source read/seek failure clears
cache, recovers the source cursor or marks it explicitly unknown, and preserves
the original exception, including non-standard exceptions. Absolute/end-relative
seek can recover; tell/relative seek cannot invent an unknown cursor.

Prefetch deliberately has a different unexpected-I/O error boundary: a small
caller request can fail because its larger cache fill touches an unreadable
range. Partial prefetch is never published and no retry hides the failure.
The paired fault control proves direct one-byte success versus a failing
16-byte prefetch. This is not exact error-timing equivalence.

## Verification

Standalone strict compilation and controls pass 2,099,669 checks/45 deliberate
rejections. They exercise actual PackageStream scalar/compact-index methods over
fake sources, cache boundaries, arbitrary valid seeks, EOF and partial reads,
offset limits, direct chunks, zero reads, denied writes and fault recovery.
Independent review found a move-then-throw seek bug; both seek paths were fixed
and re-reviewed. The selected production reader builds in a separate Release
directory and passes all 37 ordinary tests, including the table-audit SHA-256
self-test; two optional original-root integrations explicitly skip in CTest.
Both original-root integrations were then run separately and completed exit 0.

An authored 1 MiB one-byte stream required 1,048,576 source reads without cache
and 16 with it, with identical bytes, checksum and public cursor. This is a
synthetic call-count reduction only, not original-game wall-clock performance.

The Windows-only `portable_table_reader_audit` was linked to the frozen raw
library and to the new buffered library. All 41 individual table digests and
the catalog digest agree: 38 System packages plus Training, Liberty Island and
Intro, totaling 106,499 names, 114,989 exports and 5,103 imports. Canonical SHA-256
frames every returned table field, literal name spelling/order, source size and
relative path, plus the independently read complete pinned header (including
GUID/generations or the legacy heritage branch). Output contains only paths,
counts, hashes and durations, never commercial object bodies or decoded names.

The measured sum of complete loader calls was 22.930990 s raw versus 0.384153 s
buffered on this PC. This includes open/parse/validation/allocation/name
interning, not only physical I/O. OS cache and concurrent work were uncontrolled;
it is not a controlled benchmark, full startup timing or Quest performance test.
The current ASCII catalog has deterministic path encoding; non-ASCII filenames
are host-codepage dependent. File size/mtime checks are best-effort change
detection, not a concurrent snapshot. `--label` labels output; library linkage
selects the reader. The matching catalog digest is
`a74432ad57878fd1af3c697b69f648b9e562c29ccfceb0a746325af4c8a4710a`.

The buffered original actor suite passes all 1,502 Class/261 State dispatch,
2,820-statement/355-label/159-table layouts, 117 ScriptedPawns' 936 inventory
slots, actual class casts and precise unsupported Spawn278 PC253 rollback,
human/robot/bird animation, typed reference/Level/BSP Region, native physics,
state, clock and save controls. The runtime suite separately passes paired-save
recovery, retained assets, v4/v5 composition, canonical script blobs and guarded
travel. These tests do not enable Spawn or prove campaign startup. Only isolated
temporary test saves were written; original files and Quest saves were untouched.

Fresh buffered Training/static-lightmap/actor, Inventory and Health CPU captures
also pass and were visually inspected. Their mean pixel difference from the
frozen raw-reader captures is exactly zero. This is regression equivalence to
the port's own CPU previews, not to the original game or Quest GPU. The existing
one-cell inventory-icon sizing defect is unchanged here and is being corrected
in a separate source batch; fixture previews are not live inventory verification.

Do not relink outputs used by a live integration. The original raw-reader
reference remains running unchanged while the separate buffered suite passes.
Generation-scoped immutable table retention or batched inspection
may address repeated parsing separately; mutable VM hosts, transactions or save
schemas must not be reused as caches.

Ignored local evidence: `artifacts/io-buffering-20261009/` and
`artifacts/prerequisites-20261009/`. Android compilation and live deployment of
this buffering batch remain unverified. The installed test APK is unchanged.
