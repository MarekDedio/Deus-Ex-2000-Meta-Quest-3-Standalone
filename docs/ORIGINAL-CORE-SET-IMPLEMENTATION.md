# Original Core SET and app-owned User config

Implementation batch2026-10-28. This is not a completed viewport FExec chain,
initialized input, live crate bridge or new APK. Verification is recorded below
only after jobs finish; discovery errors are not passing evidence.

## Command and object semantics

Core.StaticExec101532DA..101533B6 recognizes SET and parses Class using original
ParseToken256 semantics. Actual Class lookup occurs BEFORE parsing the property;
actual inherited field lookup occurs BEFORE retaining the value. Missing Class
or property logs the original warning and is still handled. A non-SET command
declines this service; it is not silently accepted as another command.

`quest_core_set_command.h` reuses the authentic ParseCommand delimiter profile
and supplies phased256-unit tokens. Quoted overflow stops at255 retained units,
unquoted overflow consumes the full token; quoted escapes follow the original
UseEscape1 profile. The unsafe trailing-backslash/past-terminator native memory
read is explicitly refused, not padded with invented bytes. Value parsing skips
ONLY ASCII spaces after the property, preserving tabs and raw FString contents.

`portable_core_static_exec.inl` resolves actual Core.Class through prepared
ANY_PACKAGE lookup, uses the actual inherited UProperty and traverses current
indexed UObjects in ascending slot order, refreshing the upper bound. Native
Input/InputExt are admitted storage services; other selected storage/lifecycle
profiles refuse rather than being silently omitted. Default-owner aliases are
not treated as ordinary live instances.

GlobalSetProperty imports only base slot0 of a selected array property. Bare
Aliases therefore updates element0; Aliases[3] is an unrecognized field token,
not a repaired array syntax. Struct flags1 propagate nested member flags3.
Actual Alias Name/Command writes retain ordinary malformed member-prefix
stores. The authenticated flags4000 Input instances inherit PostEditChange's
false RF_Transactional gate; arbitrary transactional objects are not no-opped.

The ordered effects are live-instance imports/PostEditChange, target-CDO import,
export ALL Config fields, logical Flush(false,None), then reload CDOs under the
highest Config ancestor Engine.Input. No live-instance reload is invented.
Constructors' config pass/import/work counters remain bootstrap evidence rather
than being reused as mutable SET counters.

## Cache and transaction ownership

`OrderedIni::WithString` is immutable copy-on-write. It updates the newest
matching duplicate, retains earlier pairs/original spelling, ignores case-only
value changes and admits copied strings/vector growth before allocating. The
explicit recovered Editor config profile is not a claim about the packed retail
launcher's factory. Raw serialization emits section/key/value CRLF records and
no JSON escaping. Bytes are readable zero-extended8bit when possible, otherwise
BOM UTF16LE, preserving unpaired UTF16 units. Literal quotes/newlines retain
the original lossy parse behavior rather than introducing new escape syntax.

The runtime's native Input/default/config journal retains original owning
payloads and installs deep working copies. Rooted owners and referenced native
objects survive GC; old/new/staged bytes share the existing state budget. VM
reference writes preserve the caller's write allowance. On failure, value undo
runs before allocation-free owning-pointer swaps and before provisional object
retirement. Local import profiles borrow the caller's work/write callbacks;
those callbacks are never retained in the persistent config cache.

## Physical persistence boundary

An explicit app-private directory is bound separately from the immutable seed.
`quest_config_file_backend.h` pins checked directories and fixed Config/User.ini,
User.ini.stage and User.ini.lock paths. It rejects unsafe links/hardlinks,
overlap with the seed, stale snapshots and conflicting writers. POSIX requires
private current-uid writable ownership. Trusted Android platform paths may need
canonical physical resolution before binding; external imported assets do not
become the writable authority.

Read-only/rollback-only diagnostics NEVER prepare a stage or lock. Commit
prepares durable bytes/readback and compares the actual file snapshot at the
last fallible root boundary, after geometry/tags/collision preparation. Rename
is the irreversible boundary: NotPublished permits rollback; Published with
unknown durability is still a committed publication. No allocation or throwing
rollback decision occurs after rename. Old memory must never be rolled back
over a newly published settings file. The next transaction reacquires a fresh
snapshot before mutation and compares it with the committed byte baseline.

A fully clean, case-only/no-op cache still receives logical Flush and default
reloads but does not rewrite the file. Prepared-stage cancellation removes only
its owned stage; preexisting orphan stages are preserved until explicit locked
recovery. Original Steam files are never written.

## Integration boundary and verification

`ExecutePortableRuntimeCoreStaticExec` is an explicit original Core service.
It does NOT shortcut an unhandled Input query past RenDev/viewport/Console/
UPlayer/Engine providers. The live viewport fallback guard remains until those
providers are genuinely initialized and routed. Actual Input.Init/Reset/
UpdateInput/ReadInput and WindowReady/PreRender remain separate prerequisites
before the committing first-Training VR weapon/frame/publication bridge.

Focused runs pass: immutable config mutation17303 checks/209 refusals,
file backend223/21 and SET parser2444/225. The first backend discovery caught a
Windows destination validation handle retained across replacement; closing it
before rename resolves the nonempty-to-empty persistence test. Initial runtime
compilation also exposed late lookup-helper declarations; they were moved
before the new caller without changing their authority checks.

Final desktop/Android-native and strict ARM64 builds pass. Actual owned SET
integration passes9548 checks/6 refusals:two genuine indexed InputExt instances,
one target-CDO import,295 config exports,two CDO reloads and822 live journaled
writes, with no stage/lock in rollback-only probes. It verifies shared write/work
failure prefixes,case-only live/cache differences,malformed Alias prefix stores,
committed file publication and cold reopen. The source INI remains read-only.
An additional native lifetime check runs at final state publication and again
after fallible stage preparation,before rename.

Final CTest274 registered/229 executed/45 explicit owned-data skips/zero failures
195.29 seconds. Separate owned Input schema34355/1357 and viewport10310/20 pass;
the owned HUD retains11225 checks and original activation16025 writes/198
objects/17 visible-uninitialized nodes. Owned GC155/16,metadata13896/125 and all
eight prior first-map regressions pass (crate415/5,deferred429/21,animation588/24,
FastTrace252/0,Input classes10413/38,viewport10310/20,tutorial route1232 and
materials1308 actors/227 selected/0 null).

Original two-Fire repeat2 retains271677 instructions/32553 writes,two attack
notifies/four traces/two crate changes/nine births and exact FExec SET refusal.
Door repeat2 retains239536/19432,both56-unit moves/two endpoints and the exact
unprepared ReadInput refusal. Both verify complete rollback,not live gameplay.
Final audit:all45 originals/628 frozen compiler inputs match length,UTCmtime
and SHA256 baselines;all build/test jobs terminal. Evidence is recorded in
artifacts/core-set-final-audit-20261028.json and the corresponding build/test
logs. Initial syntax/discovery failures are retained,not counted as passing.
No resource cap,save format or accepted VR presentation layout was changed.
No new APK is packaged by this native-service batch.

Example focused verification commands (after the documented desktop build):

```powershell
artifacts/prerequisites-20261009/build/quest_native_config_mutation_test.exe
artifacts/prerequisites-20261009/build/quest_config_file_backend_test.exe
artifacts/prerequisites-20261009/build/quest_core_set_command_test.exe
artifacts/prerequisites-20261009/build/portable_core_static_exec_test.exe --game-root "D:/Steam/steamapps/common/Deus Ex"
```

The last command creates and cleans only a validated dedicated temporary app
directory. It does not bind production VR input or package/install an APK.
