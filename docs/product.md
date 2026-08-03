# Product direction

## The experience

Waypane treats a remote machine as a workspace, not a command string. An SSH action
opens a terminal without doing extra file-system work. The local/remote file
workspace appears only when explicitly requested and can be closed whenever the
terminal needs the full window. Search is the primary navigation mechanism;
nested folders and tags remain available for large fleets.

Startup opens a calm home view rather than spawning an unused shell. It offers
the selected host, a local terminal, and OpenSSH import as explicit next steps.
Global navigation does not duplicate actions already present beside the host or
workspace; application settings live in the custom window chrome.

The visual language is intentionally not a Konsole or Termius clone. It uses
layered slate surfaces, a jade connection accent, warm status colors, generous
terminal space, and a narrow tool rail. Information appears near the session it
affects rather than in global settings screens.

## First release

1. Local and SSH terminal sessions with tabs and split panes.
2. Saved hosts, nested folders, tags, keys, passwords, SSH agent, search, and OpenSSH import.
3. ProxyJump chains, agent forwarding, local/remote/dynamic tunnels.
4. On-demand two-pane SFTP with drag/drop, rsync-preferred transfers, sequential
   queueing, resumable partial files, and an integrated remote text editor.
5. Host-key verification, known-host changes, and clear connection diagnostics.
6. Workspace restoration and opt-in session logging.
7. Local stdio MCP for profile, session, tunnel, and file workflows.
8. Waypane Day and Waypane Night terminal color profiles, with automatic
   light/dark selection or an explicit user choice.
9. Built-in Ed25519 key generation and multi-forward tunnel setup from each
   connection's context menu.

## Session audit logs

SSH logging is globally opt-in. The destination is selected in Settings. Each
transcript is written beneath a sanitized host folder and named with the host,
UTC start time, and a collision-resistant suffix. Files and host folders are
owner-only. Waypane records PTY output, which captures ordinary echoed commands
and responses while respecting terminal echo suppression for password entry.
Tunnel-only sessions are not logged as interactive audit sessions.

## Later

- Mosh and serial sessions.
- Encrypted, self-hostable synchronization.
- Team vaults with scoped sharing and audit history.
- Command snippets and safe multi-host execution.
- Plugin API for inventory providers and secret managers.

## Explicit non-goals for 1.0

- A proprietary cloud account.
- AI command generation inside the terminal.
- Storing plaintext credentials in application configuration.
- Reimplementing SSH cryptography or terminal emulation from scratch.
