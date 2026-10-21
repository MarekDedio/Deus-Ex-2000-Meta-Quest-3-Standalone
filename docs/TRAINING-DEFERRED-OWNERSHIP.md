# First Training deferred Actor ownership

Work in progress,2026-10-21. Only `Maps/00_Training.dx` is integrated as actual
map content. This continues TRAINING-LIVE-LINKER.md, not completed startup,
surviving usable loot, gravity or a new playable APK.

ORIGINAL-DEFERRED-ACTOR-OWNERSHIP.md is the original static authority. The Level
decoder now reads the actual native ReachSpecs records, serialized Float time
and compact FirstDeleted reference after the existing Model prefix. Existing
model-only prefix consumers retain their old parsing boundary. No file offset,
map name or absent script tag is substituted for native queue data. Unsupported
nonnull authored queue loading refuses explicitly rather than replacing it.

Actor.Deleted is authenticated as the exact scalar const ObjectProperty with
Actor constraint. Its native storage is initialized from the genuine typed
tag/default/template value, not inferred from a missing tag or a constructor
that does not initialize the member. Map and fresh Spawn paths bind it before
callbacks; restored births validate their separately staged field before
binding, refusing unsupported nonempty links. Metadata-only fixtures without
the declaration stay unbound and cannot supply a fake None to destruction.

Original standalone DestroyActor now reads the live FirstDeleted once, stores
the captured pointer in source.Deleted, then publishes source as FirstDeleted.
Both native pointers have same-root before-images and mutable GC edges. Stores
borrow original provider native-work steps, preserving the existing VM flag/
payload journal. No source UObject lookup/index is retired and no saved slot
hole is compacted. The native queue is not a reverse child list or an arbitrary
vector reconstructed from pending-delete flags.

Changed/nonempty deferred ownership refuses checkpoint capture, restoration
and map unload/travel until faithfully archived. Ordinary insertion does NOT
call CleanupDestroyed; original ULevel.Tick reaches that later, separate
service. Its additional virtual/class-reference/scalar-destructor/hash/index
providers remain unfinished. Existing startup still explicitly refuses its
original compaction/publication boundary; no half-implemented full world is
reported as started. Native clock/state/timer APIs are not whole-world Tick.

The rollback-only native-destruction diagnostic uses real DestroyActor calls
and records before/inside/after queue state, including failures. It does not
force begun/startup flags. On a dormant loaded original it first executes the
genuine InitExecution service, preserving existing live frames rather than
fabricating a runnable state. This is an explicit diagnostic prerequisite, not
an extra production DestroyActor reset. Tests target two actual original crate instances and
LIFO insertion, exact native/linker/full-save/clock/revision/lookup rollback,
caller write/work failures and explicit archive refusals. GC comparisons after
rollback prove no leak/loss but are NOT an independent witness of queue-edge
rooting while exports also retain those actors; the Mark edges require direct
source review. The focused final-frame binary passes429 checks/21 refusal
prefixes: actual two-crate native destruction performs18 VM writes and20563
caller native-work steps. Both original returns are true; the real LIFO head/
Deleted edges and capture/restore/travel refusals pass. All40 source files
(38 shared scripts, first Training and the original ini) retain bytes/size/
timestamp, and complete native/linker/save/clock/revision/lookup/GC rollback
passes. No held-weapon hit, begun-world Destroyed callback, surviving loot or
falling is proved by this dormant, InitExecution-prepared test.

The sequential decoder's generated controls pass275 checks/90 malformed and
byte-prefix truncation refusals. Owned exact first-map admission passes824/90,
including raw original FirstDeleted byte00 at4370509,60 ReachSpecs and original
Float bits4324EE0D, with original bytes/size/timestamp unchanged.

The first general-Before queue-header extension regressed the real startup
memory limit and was rejected. The replacement journals only actual queue-edge
mutations in a bounded owning vector, with exact first before-images of BOTH
pointers and changed marker. Actual reserve capacity/peaks share the existing
state ledger; no object/work/memory limit is raised. Loader-staged authored
edges do not activate committed-state fences until successful native map
generation publication, so a failed loader can still retire its staged graph.

Startup now completes the PickupDistributor destruction and reaches a later
UNATCOTroop0 callback at PPP slot1254, ScriptedPawn.PostPostBeginPlay PC6. The
two final-frame attempts execute533447 instructions/39763 writes, using466180
cold/465808 warm native-work steps, before a new combined-memory refusal
(retained16449060, requested3192). The remaining allowance is not increased or
bypassed. Both restore full state and all40 sources; exit2 still means startup
did NOT complete. Strict ARM64 runtime/decoder/both new tests pass, and Android
native compilation/link passes for this exact source. The broad coherent
desktop rebuild and its subsequent CTest remain pending. The next memory refusal,
native cleanup, startup compaction/publication and playable crate/loot/gravity
acceptance are NOT reported as finished.

Focused evidence logs use `artifacts/training-deferred-*-frames-20261021.log`
(owned, startup, strict), `training-deferred-decode-initial-20261021.log` and
`training-deferred-android-compact-20261021.log`. The ongoing broader rebuild
uses `training-deferred-host-all-20261021.log`; it must finish before claiming
coherent full-suite regression results. No new APK is packaged or installed.
