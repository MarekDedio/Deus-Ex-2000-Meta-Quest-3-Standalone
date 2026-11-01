# Original Input AXIS coordinator (2026-11-01)

`native/quest_input_axis_command.h` implements the typed continuation selected
after the original BUTTON/PULSE/TOGGLE/AXIS branch has matched AXIS. Its caller
supplies the actual adjusted ParseCommand cursor, owning VM work/scratch ledger,
native Input/Viewport/Actor lifetime, actual FloatProperty address and ordinary
supplied output device. This header does not create any of those authorities.

The source contract and owned Engine/Core hashes are recorded in
ORIGINAL-INPUT-ACTION-COMMANDS.md. No original commercial module is loaded or
executed by this work.

## Selected control flow

- Read actual Viewport.Actor before ParseToken. Genuine initial Actor=None logs
  `Bad Axis command` through Logf with no format arguments and returns handled.
- Parse the original capacity256, UseEscape0 property token. Missing token and
  genuine FindAxisName miss take the same Logf path without parsing SPEED.
- Reread Viewport.Actor after token parsing, then retain the selected actual
  Actor/declaring FloatProperty pair. Missing, partial or foreign authority
  refuses; it never becomes a fabricated bad-command warning.
- Search the remaining command for SPEED= using the audited Core.appStrfind
  profile, not token/option recognition. The first eligible substring wins;
  ASCII letters/digits immediately before a candidate exclude it, punctuation
  and underscores do not. Absence preserves Float1. Parsing is before Action.
- Action4 performs FLD Delta / FMUL FloatSpeed / FMUL Double0.01 / FADD current
  FloatAxis / FSTP FloatAxis. Action2 omits Double0.01. The double constant is
  exactly bits3F847AE147AE147B. Other actions perform no Delta/read/store.
- All arithmetic uses the existing NativeScalar masked nearest-even PC64
  profile, including nonfinite values, subnormal values, signed zeros, retained
  intermediate precision and the one final Float store. No clamp, dead zone,
  finite-only policy, Float product spill or fused multiply-add is introduced.

## Explicit numeric profile and remaining CRT-domain limitation

The conversion first reproduces appToAnsi: at most1023 UTF16 units are copied;
units below256 become their low byte, all higher units become0x7F, and a NUL is
appended. The portable ordinary default C-locale decimal profile then accepts
leading ASCII CRT whitespace, an optional sign, decimal digits/dot and a valid
optional exponent. It converts the longest numeric prefix, so trailing junk is
not a parsing error and an incomplete exponent leaves the preceding mantissa.
Absence of a decimal prefix converts to positive zero; a genuine negative zero retains
its sign. Exponents are read completely within the1023-unit source bound.

Finite decimal conversion uses bounded fixed-limb exact positive integer
mantissa/denominator arithmetic, one nearest-even binary64 rounding and the
existing exact binary64-to-Float store. Large balanced mantissas are supported;
overflow yields signed infinity and underflow yields signed zero. This avoids
host locale, host atof/strtod, floating from_chars availability and intermediate
decimal floating multiply rounding. Simultaneous storage includes the retained
property token, ANSI buffer and conservative bounded bigint workspaces; every
selected scan/arithmetic loop borrows the owning root work limit.

This establishes an explicit **ordinary decimal conversion profile**, not a
claim of complete historical MSVCRT.atof parity. Reached textual INF/NAN,
hexadecimal input and Microsoft `1.#...` spellings explicitly refuse because
their original CRT-domain behavior is not yet established. They are not
silently interpreted with modern libc semantics or clamped to a convenient
value. Non-ASCII reached appStrfind comparisons retain that helper's existing
explicit unestablished-locale refusal. Full CRT-domain coverage remains work.

## Verification scope

Final2026-11-02 gates: generated controls pass1173 checks/233 refusals. Actual
runtime binding separately executes Axis/Hold and other commands on real player
fields at the genuine Training Init prefix (19 commands/10 stores total), then
verifies complete rollback at the still-missing Quest reconciliation boundary.
Strict ARM64, Android-native, full desktop and full CTest (259 executed/317
registered/58 explicit owned-data skips/zero failures) pass. See STATUS.md and
ORIGINAL-INPUT-RUNTIME-ACTIONS.md for exact scope; this is not playable startup.

`desktop/quest_input_axis_command_test.cpp` contains generated typed fixtures
for decimal prefix/tie/overflow/underflow/capacity behavior, source substring
search, bad-command short circuits, live Actor/Action/Delta/current reads,
selected-address retention, exact retained-product arithmetic, the established
PC64 special-value profile, missing providers and every bounded-work prefix.
Scratch limits and nested BUTTON-to-AXIS shared admission are checked too.

The fixtures are not actual RuntimeObject/Pawn metadata or original startup
proof. The owning runtime integration and final gate results must be reported
separately in STATUS.md. No test/build was started by this helper's author
before the main agent froze all compiler inputs. The header does not complete
Quest event reconciliation, Input.Init, Root.Process, ReadInput, gameplay
startup, an APK or headset acceptance.
