# Releasing Waypane

Waypane has three channels: nightly builds from `main`, prereleases such as
`v0.2.0-beta.1`, and stable `vX.Y.Z` releases. The application version is
independent of the connection-profile schema and MCP protocol versions.

## Supported packages

- Fedora KDE: native RPMs for the current and previous Fedora releases. COPR is
  the update channel; `konsole-part`, KIO SFTP, KWallet, and rsync remain native
  dependencies.
- Bazzite and other desktops: a KDE Platform Flatpak containing KonsolePart,
  the SFTP KIO worker, and Waypane's isolated OpenSSH runtime. Waypane publishes
  its own signed repository through GitHub Pages rather than Flathub.
- Source archives and a single-file Flatpak are attached to GitHub releases.

The Flatpak deliberately has no broad home or host filesystem permission. Its
local terminal is sandboxed, SSH agent access uses `ssh-auth`, and user-selected
local paths should use desktop portals. MCP clients launch the packaged server
with:

```text
flatpak run --command=waypane-mcp dev.tuska.waypane
```

## Preparing a release

1. Update `project(Waypane VERSION ...)` in `CMakeLists.txt`.
2. Add the same version as the first release in the AppStream metadata and RPM
   spec. Move relevant entries from `Unreleased` in `CHANGELOG.md`.
3. Run `./tools/check-version` and the complete test suite.
4. Push the version change through normal pull-request review.
5. Create and push a signed `vX.Y.Z` or `vX.Y.Z-beta.N` tag.

The release workflow rebuilds packages, generates checksums and provenance,
then creates a draft GitHub release. Review the draft and perform Fedora KDE and
Bazzite KDE smoke tests before approving the protected deployment environments
and publishing the release.

## Repository configuration

Create GitHub environments named `release` and `github-pages`, and require
maintainer approval for both. The draft release is created first; COPR and the
GitHub Pages Flatpak repository wait for their respective approval gates.
Protect `main` and `v*` tags, and require the CI workflow before merging.

To publish the SRPM to COPR, set:

- Repository variable `COPR_PROJECT`, for example `tuska/waypane`.
- Environment secret `COPR_CONFIG`, containing the COPR API configuration.

If `COPR_PROJECT` is absent, the release still produces RPM artifacts but safely
skips external publication.

For the Flatpak update repository, configure GitHub Pages to deploy from GitHub
Actions. If using `waypane.tuska.dev`, configure that custom domain and its DNS
in the repository's Pages settings. Then set:

- Repository variable `FLATPAK_REPO_URL`, normally
  `https://waypane.tuska.dev/repo/` or the equivalent `github.io` URL. Keep the
  trailing slash so Flatpak resolves repository objects correctly.
- Repository variable `FLATPAK_GPG_KEY_ID` with the signing-key fingerprint.
- Repository secret `FLATPAK_GPG_PRIVATE_KEY` with the armored private key. The
  reusable package workflow needs this secret to sign each Flatpak commit; the
  protected `release` environment still gates publishing the GitHub release.

The release publishes `waypane.flatpakrepo`, `waypane.flatpakref`, the exported
public key, and the signed OSTree repository. Users install it with:

```bash
flatpak install https://waypane.tuska.dev/waypane.flatpakref
```

The shared KDE runtime may still be downloaded from Flathub; the Waypane
application and its updates are served entirely by the project repository.

## Local package checks

Create an SRPM with:

```bash
./tools/make-srpm "$PWD/dist"
```

Build the Flatpak with:

```bash
flatpak-builder --force-clean --repo=flatpak-repo \
  flatpak-build packaging/flatpak/dev.tuska.waypane.yml
flatpak build-bundle flatpak-repo waypane.flatpak dev.tuska.waypane
```

OpenSSH and OpenSSL inputs are checksum-pinned. Their versions and hashes must
be updated together in the runtime builder, RPM sources, and Flatpak manifest.
