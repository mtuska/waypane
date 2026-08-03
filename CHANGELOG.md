# Changelog

All notable changes to Waypane will be documented here. The project follows
[Semantic Versioning](https://semver.org/) and uses `v`-prefixed release tags.

## Unreleased

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
