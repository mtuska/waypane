# Waypane Flatpak

This manifest is the Bazzite and cross-distribution package. It deliberately
does not grant broad home-directory access. Remote connections use Waypane's
private OpenSSH runtime, SSH agents use the `ssh-auth` socket, and user-selected
local paths should be reached through desktop portals.

The embedded local shell runs inside the Flatpak sandbox. It must be presented
as a sandbox terminal in the UI; the package does not request host-command
execution as an implicit sandbox escape.

Build from the repository root with:

```bash
flatpak-builder --force-clean --user --install-deps-from=flathub \
  flatpak-build packaging/flatpak/dev.tuska.waypane.yml
```

Release builds are GPG-signed and published as an OSTree repository through
GitHub Pages. Flathub supplies the shared KDE runtime, but Waypane itself is not
submitted to or distributed by Flathub. For a release, replace the final local
`dir` source with the tagged archive and its SHA-256 digest.
