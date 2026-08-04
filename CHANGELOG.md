# Changelog

All notable changes to Waypane will be documented here. The project follows
[Semantic Versioning](https://semver.org/) and uses `v`-prefixed release tags.

## Unreleased

## 0.1.5 - 2026-08-04

### Fixed

- Restore the Ctrl+Insert copy and Shift+Insert paste shortcuts in embedded
  terminals, which KonsolePart otherwise routes into the terminal session.

## 0.1.4 - 2026-08-03

### Added

- Select existing Waypane connections as SSH jump hosts while retaining manual
  OpenSSH aliases and ordered multi-hop chains.
- Apply the selected bastion's saved username, port, key, authentication,
  host-key, legacy-crypto, and nested jump settings from Waypane's isolated
  profile configuration, including inside Flatpak.

## 0.1.3 - 2026-08-03

### Added

- Add copy-ready MCP setup commands for Codex and Claude Code, plus a project
  `.mcp.json` template, to Settings.
- Display the installed Waypane version in Settings.
- Document how the MCP server runs from Flatpak and how to configure supported
  clients.

## 0.1.2 - 2026-08-03

### Fixed

- Preserve OpenSSH diagnostics when a connection fails instead of immediately
  closing the terminal tab.
- Suggest enabling legacy server compatibility when OpenSSH reports that no
  mutually supported algorithm could be found.

## 0.1.1 - 2026-08-02

### Fixed

- Keep local terminal processes inside the Flatpak sandbox, preventing an
  abort when opening a local shell without host-command permissions.
- Export the reverse-DNS application icon so Flatpak desktop launchers can
  display it.

## 0.1.0 - 2026-08-02

### Added

- Initial native terminal, SSH, SFTP, tunnel, logging, and MCP workspace.
