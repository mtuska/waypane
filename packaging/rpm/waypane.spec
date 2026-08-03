Name:           waypane
Version:        0.1.1
Release:        1%{?dist}
Summary:        Native workspace for SSH, SFTP, terminals, and tunnels

License:        GPL-3.0-or-later AND Apache-2.0 AND BSD-3-Clause
URL:            https://waypane.tuska.dev
Source0:        %{name}-%{version}.tar.gz
Source1:        openssh-10.4p1.tar.gz
Source2:        openssl-3.5.7.tar.gz

BuildRequires:  cmake >= 3.21
BuildRequires:  desktop-file-utils
BuildRequires:  extra-cmake-modules >= 6.0
BuildRequires:  gcc-c++
BuildRequires:  libappstream-glib
BuildRequires:  make
BuildRequires:  ninja-build
BuildRequires:  perl-interpreter
BuildRequires:  perl(Digest::SHA)
BuildRequires:  perl(File::Basename)
BuildRequires:  perl(File::Compare)
BuildRequires:  perl(File::Copy)
BuildRequires:  perl(File::Path)
BuildRequires:  perl(File::Spec::Functions)
BuildRequires:  perl(FindBin)
BuildRequires:  perl(IPC::Cmd)
BuildRequires:  perl(Module::Load::Conditional)
BuildRequires:  perl(Pod::Html)
BuildRequires:  perl(Time::HiRes)
BuildRequires:  perl(Time::Piece)
BuildRequires:  perl(bigint)
BuildRequires:  perl(lib)
BuildRequires:  qt6-qtbase-devel >= 6.5
BuildRequires:  kf6-kio-devel >= 6.0
BuildRequires:  kf6-kparts-devel >= 6.0
BuildRequires:  kf6-kwallet-devel >= 6.0
BuildRequires:  konsole-part
BuildRequires:  zlib-devel

Requires:       kio-extras
Requires:       konsole-part
Requires:       rsync
Provides:       bundled(openssh) = 10.4p1
Provides:       bundled(openssl) = 3.5.7

%description
Waypane is a Linux-native remote workspace combining managed SSH connections,
an embedded Konsole terminal, on-demand SFTP navigation, port forwarding,
session logging, and local MCP automation.

%prep
%autosetup

%build
waypane_private_openssh="$PWD/%{_vpath_builddir}/private-openssh"
# Linking every OpenSSH configure probe against an LTO static libcrypto is
# prohibitively slow. Preserve Fedora's hardening flags, but omit LTO only for
# the private runtime; the Waypane build below retains the complete RPM flags.
waypane_runtime_cflags=$(printf '%s\n' "$CFLAGS" | sed -E \
    's/(^|[[:space:]])-flto(=[^[:space:]]+)?/ /g; s/(^|[[:space:]])-ffat-lto-objects/ /g')
WAYPANE_OPENSSH_ARCHIVE=%{SOURCE1} \
WAYPANE_OPENSSL_ARCHIVE=%{SOURCE2} \
    CFLAGS="$waypane_runtime_cflags" \
    ./tools/build-private-openssh "$waypane_private_openssh"

%cmake \
    -GNinja \
    -DWAYPANE_BUILD_PRIVATE_OPENSSH=OFF \
    -DWAYPANE_PRIVATE_OPENSSH_ROOT="$waypane_private_openssh"
%cmake_build

%install
%cmake_install

%check
%ctest
desktop-file-validate %{buildroot}%{_datadir}/applications/dev.tuska.waypane.desktop
appstream-util validate-relax --nonet %{buildroot}%{_metainfodir}/dev.tuska.waypane.metainfo.xml

%files
%license LICENSE
%{_bindir}/waypane
%{_bindir}/waypane-mcp
%{_libexecdir}/waypane-ssh-helper
%{_libexecdir}/waypane-session-logger
%{_libexecdir}/waypane/openssh/
%{_datadir}/applications/dev.tuska.waypane.desktop
%{_datadir}/icons/hicolor/*/apps/dev.tuska.waypane.png
%{_metainfodir}/dev.tuska.waypane.metainfo.xml

%changelog
* Sun Aug 02 2026 Waypane Contributors <hello@tuska.dev> - 0.1.1-1
- Fix Flatpak local terminal startup and desktop icon export

* Sun Aug 02 2026 Waypane Contributors <hello@tuska.dev> - 0.1.0-1
- Initial development package
