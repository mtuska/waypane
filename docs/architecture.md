# Architecture

```text
Qt 6/KDE Widgets desktop shell
  ├─ workspace + connection UI
  ├─ TerminalAdapter ── Konsole terminal/session components
  ├─ session logger ──── nested PTY output relay (opt-in)
  ├─ FileAdapter ────── KDE KIO SFTP worker
  └─ Waypane Core
       ├─ profile store (non-secret JSON)
       ├─ secret references ── KWallet
       ├─ SSH option resolver
       └─ policy + audit boundary
                    ▲
                    │ user-only Unix socket / shared profile format
stdio MCP server ───┘
```

## Boundaries

`src/core` has no UI dependency. It owns connection schema, validation, storage,
and construction of argument vectors. Commands are never assembled for a shell.

`src/app` is the native desktop client. Qt Widgets own the custom frame and
workspace. KonsolePart supplies terminal emulation; KDirOperator and KIO supply
SFTP navigation, queued file jobs, and remote text-file reads/writes. Compatible
key-based transfers prefer a sequential rsync path with partial-file verification;
the queued request captures its profile and alias so switching the visible host
cannot redirect an already queued transfer.

`tools/waypane-mcp` is a local stdio server. It deliberately reads the same
non-secret profile document and never reads the desktop keyring. A user-only Unix
socket lets it ask the running GUI to open SSH, SFTP, or local terminal views.
Those actions remain distinct.

## Konsole relationship

Waypane is not a fork of the checked-out application. It loads the installed
KonsolePart through its public KParts/TerminalInterface boundary. This preserves
mature emulation behavior while Waypane's application model and interface remain
independent.

## SSH isolation

The default build produces a checksum-pinned portable OpenSSH client linked to a
private static OpenSSL build, stages it beside the development executable, and
installs it under `libexec/waypane/openssh`. Modern profiles do
not enable retired algorithms. Legacy compatibility is an explicit per-profile
exception; if the private runtime is absent, the connection is refused instead
of weakening or relying on the host operating system's SSH policy.

## Workspace lifecycle

Each terminal tab owns a QSplitter and one or more independent KonsolePart
instances. Window geometry, local pane counts, split orientation, and the selected
profile are persisted. SSH pane reconnection is disabled by default and requires
an explicit setting; the SFTP workspace is never reopened implicitly.

KonsolePart loads Waypane-managed Day and Night profiles from the user's standard
Konsole data location. These profiles change only terminals embedded in Waypane;
they do not replace the user's Konsole defaults. When auditing is enabled, SSH is
started through `waypane-session-logger`, which relays a nested PTY byte-for-byte
and writes output to an owner-only transcript. Secrets remain outside arguments
and password input remains protected by the child PTY's echo setting.

## Data

Profiles live at `$XDG_CONFIG_HOME/waypane/connections.json` (normally
`~/.config/waypane/connections.json`). Secret fields contain only opaque keyring
references. Writes are atomic. The schema is versioned from its first release.
