# Waypane

Waypane is an open-source, Linux-native workspace for terminal sessions, SSH
connections, tunnels, and SFTP. It combines Konsole's dependable native terminal
foundation with the connection-centric workflow of tools such as Termius, while
remaining local-first and automation-friendly.

The application ID is `dev.tuska.waypane`; the project site will live at
`https://waypane.tuska.dev`.

## Product principles

- Native first: Qt 6, Wayland/X11, desktop keyring integration, no browser shell.
- One host, one workspace: terminal, remote files, tunnels, and history share a
  connection profile.
- A focused home screen opens first; terminals and SFTP are created only when
  requested.
- Waypane Day/Night terminal palettes follow the application theme or can be
  selected independently.
- Optional owner-only per-host session transcripts use UTC filenames for audit
  retention, with a configurable destination.
- Generate Ed25519 identities in the connection editor and configure multiple
  local, remote, or SOCKS forwards from a host's context menu.
- Open formats: import OpenSSH config and keep non-secret metadata portable.
- Secrets stay secret: passwords and key passphrases belong in the OS keyring,
  never in JSON, command arguments, logs, or MCP responses.
- Automation with consent: MCP can inspect and manage profiles; remote execution
  and file writes will require explicit policy or approval.

## Current milestone

The repository contains a buildable native application with its own client-side
window frame, persistent system/light/dark themes, an original Waypane icon,
hierarchical connection folders and tags, OpenSSH import, KWallet secrets,
embedded Konsole terminals, an on-demand local/remote SFTP workspace, and a local
stdio MCP server with a user-only live-control socket. SSH and SFTP are deliberately
separate actions. File transfers prefer rsync when compatible credentials are in
use, with resumable flags and a sequential queue; KDE's KIO handles the fallback
path. Terminal tabs support right/down splits, local layouts survive restarts, and
the file workspace includes a native UTF-8 remote editor. Restoring remote SSH
panes is opt-in so startup does not create unexpected network connections.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/src/waypane
```

Fedora development packages include `extra-cmake-modules`, `kf6-kio-devel`,
`kf6-kparts-devel`, and `kf6-kwallet-devel`.

## Isolated OpenSSH runtime

Normal builds produce and stage Waypane's own OpenSSH client and crypto runtime.
The build downloads checksum-pinned upstream OpenSSH 10.4p1 and OpenSSL 3.5.7
sources, then produces a relocatable client toolset with libcrypto linked
statically. The development executable discovers it under `build/src/openssh`.
Use an existing prepared runtime with:

```bash
cmake -S . -B build \
  -DWAYPANE_BUILD_PRIVATE_OPENSSH=OFF \
  -DWAYPANE_PRIVATE_OPENSSH_ROOT=/absolute/path/to/waypane-openssh
```

For quick offline UI-only builds, pass
`-DWAYPANE_BUILD_PRIVATE_OPENSSH=OFF` without a runtime path. Legacy profiles
will remain intentionally unavailable in that configuration.

Weak algorithms are disabled for ordinary profiles. The per-profile legacy mode
adds only the explicitly supported RSA/SHA-1, group1/group14, CBC, and HMAC-SHA1
options, and Waypane refuses to use the system SSH binary for such a profile.

Run the MCP server directly:

```bash
./tools/waypane-mcp
```

See [the architecture](docs/architecture.md), [product direction](docs/product.md),
and [MCP contract](docs/mcp.md).

## Packages and releases

Waypane uses semantic versions from the top-level CMake project and `vX.Y.Z`
release tags. Fedora KDE is packaged as a native RPM, while Bazzite and other
distributions use the KDE Platform Flatpak. The Flatpak's local shell is
intentionally sandboxed; remote SSH uses the bundled client and selected local
paths are expected to use desktop portals.

Waypane itself is distributed from GitHub rather than Flathub. GitHub Releases
carry RPMs, a standalone Flatpak bundle, source archives, checksums, and build
provenance. GitHub Pages serves the signed Flatpak update repository; after the
first release, users can install it with:

```bash
flatpak install https://waypane.tuska.dev/waypane.flatpakref
```

Flathub is used only as the source of the shared KDE runtime.

GitHub Actions test the current and previous Fedora releases, build RPM and
Flatpak artifacts, verify the private OpenSSH runtime weekly, and prepare an
attested draft release for protected version tags. See the complete
[release guide](docs/releasing.md).

## Reference checkout

`konsole/` is an ignored, read-only upstream checkout used for architectural
research. Waypane code lives outside it and does not modify or vendor that tree.

## License

GPL-3.0-or-later. This is compatible with future reuse of GPL Konsole components;
reused code must retain its original notices and attribution.
