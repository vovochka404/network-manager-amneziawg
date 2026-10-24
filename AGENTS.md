# Agent Instructions for NetworkManager-amneziawg

## Overview

NetworkManager-amneziawg is a C VPN plugin for NetworkManager providing client-side AmneziaWG support. It consists of a service binary, a connection-editor plugin, and shared utility code.

## Quick Start

### Build & Install
```bash
# Quick build (auto-detects GTK3/GTK4)
./scripts/build.sh

# Clean rebuild
./scripts/build.sh --clean

# Install
sudo cmake --install build
```

### RPM on Fedora (optional helper)
```bash
make -C packaging/fedora deps   # build dependencies (dnf), once
make -C packaging/fedora rpm    # both editors, tests, package in build/rpm/
```
Fedora-only convenience, also for Fedora Atomic inside a toolbox; see
`packaging/fedora/README.md`. The generic build is CMake/CPack below.

### Check Code Style
```bash
# Check
./scripts/check-style.sh

# Auto-fix
./scripts/check-style.sh --fix
```

### Run Tests
```bash
cd build
ctest --output-on-failure
```

## Build & Install (CMake)

### Manual Build
```bash
mkdir build && cd build
cmake ..  # auto-detects GTK3/GTK4 from pkg-config
cmake --build .  # .gmo translation files are auto-generated
sudo cmake --install .  # requires root
```

### CMake Options
| Option | Description | Default |
|--------|-------------|---------|
| `WITH_GTK3` | GTK3 editor (requires libnma): `AUTO`, `ON` or `OFF` | `AUTO` |
| `WITH_GTK4` | GTK4 editor (requires libnma-gtk4 and `gtk4-builder-tool`): `AUTO`, `ON` or `OFF` | `AUTO` |
| `CMAKE_INSTALL_PREFIX` | Install prefix | `/usr` |
| `CMAKE_INSTALL_LIBDIR` | Library directory (e.g., lib64) | GNUInstallDirs |

`AUTO` builds an editor when its dependencies are found, `ON` fails the configure
step without them, `OFF` leaves it out. Each option is independent, and its value
stays in the cache, so re-running `cmake` in the same build directory keeps the
choice (an `option()` cached the first auto-detected `OFF`, which a re-run then took
for the user's). The resolved result is `BUILD_GTK3_EDITOR`/`BUILD_GTK4_EDITOR`;
CMake code tests those, not `WITH_GTK*`. Build trees configured while these were
`option()`s still hold that `BOOL` `OFF`; they are recognised by the old help string
(`Build with GTK3 support ...`), reset to `AUTO` once and warned about. A `-D` on the
command line replaces the help string, so an explicit `OFF` on that run is kept. An
`ON` from those trees (or a `-DWITH_GTK3:BOOL=ON`) keeps its value, but `set(CACHE)`
does not touch the type of an existing entry, so every run sets `TYPE STRING`, the
new `HELPSTRING` and `STRINGS` explicitly; otherwise ccmake and cmake-gui would show
a `BOOL` checkbox without `AUTO`.

`CMAKE_INSTALL_PREFIX` defaults to `/usr` (NetworkManager loads plugins from its own
directories only); an explicit prefix is kept. The `program=` and `auth-dialog=`
paths in `nm-amneziawg-service.name` come from `CMAKE_INSTALL_FULL_LIBEXECDIR`, so
`GNUInstallDirs` is included only after the prefix is settled, and CPack packages
into the same prefix (`CPACK_PACKAGING_INSTALL_PREFIX`). The `.name` file itself goes
to libnm's `vpnservicedir`, which does not depend on the prefix. Neither does the
D-Bus policy: the system bus reads `system.d` only below its own data directory and
`/etc` (dbus-daemon refuses the service name with `AccessDenied` otherwise), so it goes
to `DBUS_SYSTEM_POLICY_DIR`, from `pkg-config --variable datadir dbus-1` (normalised,
Fedora's is `${pcfiledir}/../..`), or libnm's prefix + `/share` without `dbus-1.pc`.
`NM_PLUGIN_DIR` stays empty in the cache unless set and is then derived from
`CMAKE_INSTALL_FULL_LIBDIR` on every run, so it follows a changed prefix. `LOCALEDIR`
is `CMAKE_INSTALL_FULL_LOCALEDIR`.

`project()` enables C only: there are no C++ sources, so no C++ compiler is needed.

### Version Detection

The project version is automatically detected from git tags:

- **With git tag** (e.g., `v1.2.3`): Version = `1.2.3` (with `-dirty` suffix if uncommitted changes)
- **Without git tag**: Version = `0.9.5-dev-<commit-hash>` (default version with commit hash)

```bash
# Create a release tag - version will be extracted from tag
git tag v1.2.3

# Build will use version from tag
mkdir build && cd build
cmake ..
# PROJECT_VERSION_FULL = 1.2.3 (or 1.2.3-dirty if working directory has changes)
```

### Build Examples
```bash
# Force GTK3 only
cmake .. -DWITH_GTK3=ON -DWITH_GTK4=OFF

# Force GTK4 only
cmake .. -DWITH_GTK3=OFF -DWITH_GTK4=ON

# Build without editor (service only)
cmake .. -DWITH_GTK3=OFF -DWITH_GTK4=OFF

# System installation (for RPM packages)
cmake .. -DWITH_GTK3=ON -DWITH_GTK4=ON -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib64
cmake --build .
cpack -G RPM

# Build DEB package (Debian/Ubuntu)
cmake .. -DWITH_GTK3=ON -DWITH_GTK4=ON -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib
cmake --build .
cpack -G DEB
```

Package dependencies: rpmbuild adds the shared-library requirements by itself;
the DEB gets them from `dpkg-shlibdeps` (`CPACK_DEBIAN_PACKAGE_SHLIBDEPS`, needs
`dpkg-dev`), which finds the private core library in the staged package through its
`RUNPATH` (`$ORIGIN`). `CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS` (CMake 3.20+) is
therefore not needed; checked with CMake 3.13, 3.18, 3.22 and 3.28. Only
`network-manager` is listed by hand.

### Library Architecture

The project uses a shared library for common code:

```
libnm-vpn-plugin-amneziawg-core.so  ← AWGDevice, AWGConfig, etc.
    ↓                                    ↓
libnm-vpn-plugin-amneziawg.so     libnm-vpn-plugin-amneziawg-editor.so (GTK3)
                                  libnm-gtk4-vpn-plugin-amneziawg-editor.so (GTK4)
```

The service (`nm-amneziawg-service`) and the auth dialog helper
(`nm-amneziawg-auth-dialog`) are executables that compile the shared sources they
need themselves and do not link the core library.

**Why:** Prevents code duplication and ensures GObject type registration happens in a single library, avoiding deadlock issues with `g_once_init_enter()` when types are registered in multiple libraries loaded via dlopen.

**RPATH Configuration:**
```cmake
set(CMAKE_BUILD_RPATH "$ORIGIN")
set(CMAKE_INSTALL_RPATH "$ORIGIN")
```

All libraries have `RUNPATH=$ORIGIN`, allowing them to find each other when installed to the same directory (`/usr/lib/NetworkManager/`).

**No version script for core library:** It's internal-only, not exposed as public API.

## Testing
```bash
# Configure and build
mkdir build && cd build
cmake ..

# Run all tests
ctest

# Run specific test
ctest -R amneziawg-test     # tests/test-main.c: shared/awg units, config files, connection managers with stub tools
ctest -R auth-dialog-test   # tests/test-auth-dialog.c: runs the built nm-amneziawg-auth-dialog with crafted agent input

# Build and run test executable directly
cmake --build . --target nm-amneziawg-test
cd .. && ./build/nm-amneziawg-test

# Verbose output
ctest --output-on-failure --verbose
```

### Writing Tests for shared/awg Module
Tests use GLib test framework (`g_test_init()`, `g_test_add_func()`).

**Test file structure** (`tests/test-main.c`):
```c
#include <glib.h>
#include "awg/awg-config.h"
#include "awg/awg-device.h"

static gchar *
get_test_config_path(const gchar *filename)
{
    return g_build_filename(g_get_current_dir(), "tests", filename, NULL);
}

static void
test_example(void)
{
    gchar *config_path = get_test_config_path("test-config.conf");
    AWGDevice *device = awg_device_new_from_config(config_path);
    
    g_assert_nonnull(device);
    // ... assertions ...
    
    g_object_unref(device);
    g_free(config_path);
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/path/to/test", test_example);
    return g_test_run();
}
```

**Adding test config files**: Place `.conf` files in `tests/` directory. CMake automatically copies them to `build/tests/`.

**CMake test configuration**:
```cmake
file(GLOB TEST_CONFIGS "${CMAKE_SOURCE_DIR}/tests/*.conf")
foreach(config ${TEST_CONFIGS})
    get_filename_component(config_name ${config} NAME)
    configure_file(${config} ${CMAKE_BINARY_DIR}/tests/${config_name} COPYONLY)
endforeach()

add_test(NAME amneziawg-test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} COMMAND nm-amneziawg-test)
```

**Important notes**:
- Tests must run from project root (uses `g_get_current_dir()`)
- Config files are copied to `build/tests/` at build time
- Use `g_assert_*()` macros for assertions
- Test functions take no arguments and return void
- Use `g_object_unref()` for GObject cleanup
- `g_test_init()` makes warnings fatal: a test that triggers a setter's `g_warning()` wraps the call in `g_test_expect_message()` / `g_test_assert_expected_messages()`
- Connection managers are tested with stub `awg-quick`/`awg` scripts reached through `NM_AWG_QUICK_PATH` and `PATH`; the external manager takes a writable `config-dir` instead of `/run/nm-amneziawg` (see `ExternalStubs` in `tests/test-main.c`)

### Common Testing Patterns
```c
// Test subnet parsing
AWGSubNet *subnet = awg_subnet_new_from_string("10.0.0.1/24");
g_assert_nonnull(subnet);
g_object_unref(subnet);

// Test peer endpoint
AWGDevicePeer *peer = awg_device_peer_new();
g_assert_true(awg_device_peer_set_endpoint(peer, "vpn.example.com:51820"));
g_assert_cmpstr(awg_device_peer_get_endpoint(peer), ==, "vpn.example.com:51820");
    g_object_unref(peer);
}

// Test round-trip (write and read)
gboolean saved = awg_device_save_to_file(device, output_path);
g_assert_true(saved);
AWGDevice *device2 = awg_device_new_from_config(output_path);
g_assert_true(awg_device_is_valid(device2));
```

### Validator Functions

Validators are defined in `shared/awg/awg-validate.c`.

**Available validators:**
| Function | Description | Range/Limits |
|----------|-------------|--------------|
| `awg_validate_base64()` | Base64 alphabet and padding only | No length check |
| `awg_validate_key()` | A WireGuard key (private, public, preshared, HeaderProtectionKey): what the `AWGDevice` key setters accept | Base64, exactly 32 bytes decoded |
| `awg_validate_endpoint()` | Validates host:port format | - |
| `awg_validate_mtu()` | Validates MTU value | 0 (auto), 68-1500 |
| `awg_validate_port()` | Validates port number | 0-65535 |
| `awg_validate_fw_mark()` | Validates fwmark value | 0-UINT32_MAX |
| `awg_validate_i_packet()` | Validates i1-i5 format | Tags: r, rc, rd, c, t, b; space required, N ≤ 65535 |
| `awg_validate_s3()` | Validates s3 junk size | 0-65479 (kernel limit) |
| `awg_validate_s4()` | Validates s4 junk size | 0-65503 (kernel limit) |
| `awg_validate_junk_size()` | Validates jc, jmin, jmax, s1, s2 | 0-65535 |
| `awg_validate_magic_header()` | Validates h1-h4 format | Single number or range start-end |
| `awg_validate_magic_headers_no_overlap()` | Validates H1-H4 ranges don't overlap | - |
| `awg_validate_jmin_jmax()` | Validates jmin <= jmax | - |
| `awg_validate_allowed_ips()` | Validates allowed IPs subnets | - |
| `awg_magic_header_parse()` | Parses magic header to start/end | - |
| `awg_range_parse_u32()` | Packs a u16 range (`lo-hi` or `lo`) the way the kernel stores it | `hi<<16 \| lo`, components 0-65535 |
| `awg_version_parse()` | Parses `major[.minor]` out of a version string | Leading non-digits skipped, so module versions and the tools banner both parse |
| `awg_version_at_least()` | Feature gating against a minimum version | Unknown/unparseable version counts as supported |

**Removed unused validators:** `awg_validate_dns`, `awg_validate_keep_alive`, `awg_validate_ip_or_empty`, `awg_validate_header_size`

### String to Number Conversion

Use GLib functions for string to number conversion instead of standard C functions:

**Use:**
- `g_ascii_string_to_unsigned()` - for converting strings to unsigned integers
- `g_ascii_string_to_signed()` - for signed integers

**Avoid:**
- `strtoul()`, `strtoull()`, `strtol()`, `strtoll()`
- `strtod()`, `strtof()`

Example:
```c
guint64 value;
if (!g_ascii_string_to_unsigned(str, 10, 0, 65535, &value, NULL)) {
    g_warning("Invalid value: %s", str);
    return FALSE;
}
```

Benefits:
- Consistent error handling with g_warning
- Range validation built-in
- No need to check errno
- Clear return value (boolean)

## Localization (i18n)

Translations are stored in `po/` directory with `.po` files. CMake automatically handles everything.

### Build Process
- `.po` files are auto-detected via `file(GLOB ...)` in CMakeLists.txt
- `.gmo` (binary machine object) files are generated from `.po` during build
- `.pot` template is generated using xgettext with `-k_` flag
- POT file uses **relative paths** for source file references (e.g., `properties/nm-amneziawg-editor.c:70` instead of absolute paths)

### Marking Strings for Translation
In C source files, wrap user-visible strings with `_()`:
```c
g_string_append(error_msg, _("Public key is required and must be valid base64\n"));
```

In UI files (.ui), use `translatable="yes"` property:
```xml
<property name="tooltip-markup" translatable="yes">Base64-encoded 32-byte private key.</property>
```

**Important:** When translating strings that contain `<` or `>` characters (e.g., I1-I5 format descriptions), escape them as `&lt;` and `&gt;` in the .po file. Otherwise GTK will fail to parse the markup and show warnings:
```
Failed to set text from markup due to error parsing markup
```

Example in .po file:
```po
msgid "Format: tags separated by < (e.g., r16<c)"
msgstr "Формат: теги через &lt; (например, r16&lt;c)"
```

### Adding a New Language
1. Add translation source files to `po/POTFILES.in`
2. Run: `msginit -i po/NetworkManager-amneziawg.pot -o po/ru.po -l ru`
3. Edit the new `po/ru.po` file
4. Rebuild to generate `.gmo`

### Updating Translations
When source strings change:
1. Rebuild to regenerate `po/NetworkManager-amneziawg.pot`:
   ```bash
   cd build && cmake --build . --target pot
   ```
2. Copy new pot to po directory:
   ```bash
   cp build/NetworkManager-amneziawg.pot po/
   ```
3. Update each language: `msgmerge -U po/de.po po/NetworkManager-amneziawg.pot`
4. Add translations to the updated .po files
5. Rebuild: `cd build && cmake --build .`

### Checking for Untranslated Strings
To find truly untranslated strings (excluding header), use:
```bash
cd po && msgattrib --untranslated de.po | grep "^msgid" | grep -v 'msgid ""'
```

### Files
- `po/POTFILES.in` - list of source files to translate
- `po/NetworkManager-amneziawg.pot` - template (generated)
- `po/*.po` - translation files (de, en_GB, zh_CN, etc.)
- `po/*.gmo` - binary translations (generated in build/)

## CI/CD

### GitHub Actions Workflows

The project uses GitHub Actions for continuous integration and delivery:

**CI Checks** (`.github/workflows/ci.yml`) - Runs on every PR and push:
- Code style: `./scripts/check-style.sh` with the clang-format of `ubuntu-latest` (18 on Ubuntu 24.04), so every `.c`/`.h` below `src/`, `shared/`, `properties/` and `tests/` is covered without listing files
- Build on Ubuntu (GTK3 + GTK4) + tests + DEB artifact; prints the DEB's `Depends`
- Build on Fedora (GTK3 + GTK4) + tests + RPM artifact

**Release Packages** (`.github/workflows/release.yml`) - Runs on release:
- Builds RPM (Fedora) and DEB (Ubuntu 24.04, pinned) packages, both editors in each
- Uploads to GitHub Releases
- Can be triggered manually via workflow_dispatch

The GTK4 editor needs `gtk4-builder-tool` at build time: `gtk4-devel` on Fedora,
`libgtk-4-bin` on Ubuntu. The DEB's library dependencies come from `dpkg-shlibdeps`
(`dpkg-dev`), so the release DEB installs only on the Ubuntu release it was built on
and newer ones; that is why `build-deb` is pinned to `ubuntu-24.04`.

The Fedora jobs run in a `fedora:latest` container, which has no git: they install
it *before* `actions/checkout` (otherwise the checkout is a tarball without `.git`)
and then add the workspace to `safe.directory` (it belongs to the runner user, not
to root in the container). Without either, `git describe` fails and the package
gets the default version 0.9.5.

### Local Pre-commit Checks

Before committing changes, run:
```bash
./scripts/check-style.sh
./scripts/build.sh --clean
```

### Building Packages Locally

```bash
# DEB package
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib
cmake --build .
cpack -G DEB

# RPM package
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib64
cmake --build .
cpack -G RPM
```

On Fedora, `make -C packaging/fedora rpm` does the RPM steps with both editors
(optional helper, see `packaging/fedora/README.md`).

## Linting
```bash
# Check specific files
clang-format --style=file --dry-run --Werror src/file.c

# Fix specific files
clang-format --style=file -i src/file.c

# Check all source files
./scripts/check-style.sh

# Auto-fix all files
./scripts/check-style.sh --fix
```

## Code Style
- **Formatting**: clang-format GNU style, 4-space indent, column limit 80. See `.clang-format`.
- **Brace style**: after class/function, never after control statements, no space before `else`.
- **Naming**: functions `lowercase_with_underscores`; types `PascalCase` with `AWG` prefix; constants `UPPER_SNAKE`; files `lowercase-hyphens.c`.
- **Headers**: system headers first, then local; angle brackets for system, quotes for project. Keep includes minimal - remove unused headers.
- **Error handling**: GLib `GError` patterns, `g_warning()` for non-fatal, `g_error()` for fatal.
- **Memory**: GObject reference counting, GLib free functions, NULL checks.
- **Types**: GLib types (`gchar`, `gboolean`, `guint32`), prefer `const`.

## Best Practices

### CMake Variables
- Use CMake standard variables from `GNUInstallDirs` module instead of constructing paths from `CMAKE_INSTALL_PREFIX`
- Include `include(GNUInstallDirs)` in CMakeLists.txt
- Use: `CMAKE_INSTALL_LOCALSTATEDIR`, `CMAKE_INSTALL_SYSCONFDIR`, `CMAKE_INSTALL_DATADIR`, etc.

### Temporary Files
- Use `g_get_tmp_dir()` from GLib instead of hardcoded `/tmp/`
- This ensures proper handling across different systems and environments
- Files the service needs for the lifetime of a connection go to `/run/nm-amneziawg`, not `/tmp`: NetworkManager.service has `PrivateTmp=true`, and that `/tmp` is gone when the service tears a tunnel down after NetworkManager stopped (see "External Implementation (awg-quick)")

### Includes
- Keep includes minimal - only include what's actually used
- Remove unused system and local includes to improve compile times
- Group includes: system headers first, then project headers

## Directory Layout

```
├── src/              # Service binary (nm-amneziawg-service), auth dialog helper (nm-amneziawg-auth-dialog)
├── properties/       # Connection editor plugin (GTK3/GTK4)
├── shared/           # Shared utility code
│   ├── awg/          # AmneziaWG core module
│   ├── nm-utils/     # NetworkManager utilities
│   └── amneziawg.c   # Netlink helper
├── tests/            # Unit tests using GLib test framework
├── po/               # Translations
├── scripts/          # Helper scripts (build.sh, check-style.sh, fix-pot-paths.sh, gtk3to4-ui.sh)
├── packaging/        # Distribution helpers: arch/PKGBUILD, fedora/Makefile (optional RPM build)
├── .github/
│   └── workflows/    # GitHub Actions CI/CD workflows
├── appdata/          # AppData/metainfo files
├── build/            # Build directory (git-ignored)
└── .cache/           # Cache directory (git-ignored)
```

## Development Scripts

| Script | Description |
|--------|-------------|
| `scripts/build.sh` | Build and test script |
| `scripts/check-style.sh` | Code style checker (clang-format) |
| `scripts/fix-pot-paths.sh` | Post-process POT file to use relative paths |
| `scripts/gtk3to4-ui.sh` | Build step: converts the GTK3 dialog into the GTK4 one (see "GTK3/GTK4 Compatibility") |

### Build Script Usage
```bash
./scripts/build.sh              # Default build (every editor whose dependencies are installed)
./scripts/build.sh --gtk3       # GTK3 editor only
./scripts/build.sh --gtk4       # GTK4 editor only
./scripts/build.sh --minimal    # Build without editor (service only)
./scripts/build.sh --clean       # Clean and rebuild
```

### Style Check Usage
```bash
./scripts/check-style.sh        # Check all source files
./scripts/check-style.sh --fix  # Auto-fix style issues
```

## Runtime Requirements
- A backend: the `amneziawg` kernel module (netlink), or **amneziawg-tools** (`awg-quick`) with the module or the `amneziawg-go` userspace implementation; see "Connection Manager Module"
- D‑Bus policy is installed to the system bus policy directory (`DBUS_SYSTEM_POLICY_DIR`, usually `/usr/share/dbus-1/system.d`) whatever the prefix
- The `.name` file goes to libnm's `vpnservicedir` (usually `/usr/lib/NetworkManager/VPN`), the plugin libraries to `${libdir}/NetworkManager`, the service and the auth dialog to `${libexecdir}`

## References
- https://developer.gnome.org/NetworkManager/stable/gdbus-org.freedesktop.NetworkManager.VPN.Plugin.html
- https://developer.gnome.org/glib/stable/glib-Error-Reporting.html

## shared/awg Module

### Architecture
```
WireGuard config file <-> AWGDevice <-> NMConnection
```

The plugin has two modes of operation:
1. **Service (src/)**: Uses AWGDevice for full tunnel configuration
2. **Editor (properties/)**: Works directly with NMSettingVpn for VPN-specific settings; AWGDevice used only for peer management

### Important: IP/DNS Configuration

IP addresses and DNS servers are configured via **standard NetworkManager tabs** (IPv4 Settings, IPv6 Settings), NOT in the VPN plugin editor. This follows the standard NetworkManager VPN plugin pattern.

The VPN editor handles only:
- Private key (required)
- Listen port
- MTU, FwMark
- AmneziaWG parameters (jc, jmin, jmax, s1-s4, h1-h4, i1-i5)
- Peers (via dialog)

### Files

- `shared/awg/awg-device.h/c` - GObject-based intermediate representation
  - `AWGDevice` - interface config (private/public keys, address, DNS v4/v6, port, mtu, fw_mark, AmneziaWG params)
  - `AWGDevicePeer` - peer config (public key, preshared key, endpoint, allowed IPs, keep-alive, advanced security)
  - `AWGSubNet` - subnet for allowed IPs (address + mask)
  - Keys stored as raw 32-byte binary internally, converted to/from base64
  - `awg_device_is_valid()` / `awg_device_peer_is_valid()` for validation
  - DNS split into `dns_v4` and `dns_v6` lists

- `shared/awg/awg-config.h/c` - WireGuard config file format conversion
  - `awg_device_new_from_config()` - parse `[Interface]`/`[Peer]` sections
  - `awg_device_create_config_string()` - generate WireGuard config format
  - `awg_device_save_to_file()` - save to file
  - DNS automatically splits by IP type on import, combines on export
  - The generated file is what `awg setconf` parses for the awg-quick backend, so it carries only keys the tools know: `Jc`..`S2` always (every AmneziaWG version has them); `S3`/`S4`, `PersistentKeepalive`, `ListenPort`, `FwMark`, `MTU` only when non-zero (parsers without S3/S4 reject the keys, and amneziawg-tools 3.0+ parse `PersistentKeepalive` as a number range, so `off` is a `Configuration parsing error`); string and AWG 3.1 keys only when set. Import still accepts `PersistentKeepalive = off`.

### AmneziaWG Parameters

**Numeric parameters:**
| Parameter | Description | Range |
|-----------|-------------|-------|
| jc | Junk packet count | 0-10 |
| jmin | Minimum junk packet size | 64-1024 bytes |
| jmax | Maximum junk packet size | 64-1024 bytes |
| s1 | First noise packet size (init_packet_junk_size) | 0-64 bytes |
| s2 | Second noise packet size (response_packet_junk_size) | 0-64 bytes |
| s3 | Cookie reply packet junk size | 0-64 bytes |
| s4 | Transport packet junk size | 0-32 bytes |

**String parameters (max 5KB each):**
| Parameter | Description | Notes |
|-----------|-------------|-------|
| h1 | First handshake packet magic header | Arbitrary string |
| h2 | Second handshake packet magic header | Arbitrary string |
| h3 | Third handshake packet magic header | Arbitrary string |
| h4 | Fourth handshake packet magic header | Arbitrary string |
| i1 | First init packet content | CPS format |
| i2 | Second init packet content | CPS format |
| i3 | Third init packet content | CPS format |
| i4 | Fourth init packet content | CPS format |
| i5 | Fifth init packet content | CPS format |

**Important:** H1-H4 and I1-I5 are stored as strings, not numbers. Use `awg_validate_awg_string()` for validation (max 5KB).

### AmneziaWG Parameters Synchronization

**Important:** AmneziaWG parameters must be coordinated between client and server for the obfuscation to work.

| Parameter | Must Match Server | Notes |
|-----------|-------------------|-------|
| **s1, s2, s3, s4** | Yes | Noise packet sizes |
| **h1, h2, h3, h4** | Yes | Magic headers |
| **i1, i2, i3, i4, i5** | Yes | Init packet contents |
| **AdvancedSecurity** | Yes | Peer flag |
| **jc, jmin, jmax** | No | Can be different on client |

**Parameters that can differ:**
- Address (each side has its own IP)
- ListenPort (client uses random, server uses fixed)
- DNS (each side configures its own)
- Endpoint (client points to server)
- AllowedIPs (usually different)
- MTU, FwMark (can differ)
- KeepAlive (only outgoing peer needs it)

**UI implication:** When editing a client connection, s1-s4, h1-h4, i1-i5, and AdvancedSecurity should match the server. Consider adding a tooltip or warning when these parameters differ from defaults.

### AmneziaWG Parameters Data Format (Kernel Reference)

Reference implementation is in `/usr/src/amneziawg-1.0.20260210/`.

**WARNING:** The kernel's `WG_GENL_VERSION` is 2 (see `uapi/wireguard.h`), not 1. Ensure `shared/amneziawg.c` uses version 2.

#### H1-H4: Magic Headers

Format: single number or range `start-end`

Source: `magic_header.c:mh_parse()` (lines 8-32)

| Input | Result | start | end |
|-------|--------|-------|-----|
| `"1"` | OK | 1 | 1 |
| `"42"` | OK | 42 | 42 |
| `"1-100"` | OK | 1 | 100 |
| `"100-1"` | ERROR (start > end) | - | - |
| `"abc"` | ERROR (not a number) | - | - |
| `"1-"` | ERROR (empty after -) | - | - |
| `""` | ERROR (empty) | - | - |

**Validation:** Kernel checks that H1-H4 ranges do NOT overlap (`device.c:604-612`).

If ranges overlap, kernel returns:
```
<HWNAME>: H1 and H2 ranges must not overlap
<HWNAME>: H2 and H3 ranges must not overlap
...
```

**Default values** (WireGuard message types):
- H1 = 1 (MESSAGE_HANDSHAKE_INITIATION)
- H2 = 2 (MESSAGE_HANDSHAKE_RESPONSE)
- H3 = 3 (MESSAGE_HANDSHAKE_COOKIE)
- H4 = 4 (MESSAGE_DATA)

**When reading from kernel:** Use `mh_genspec()` which outputs:
- `"42"` if start == end
- `"1-100"` if start != end

#### S3/S4: Junk Sizes (uint16)

| Parameter | Description | Theoretical max | Kernel max |
|-----------|-------------|-----------------|-----------|
| s1 | init_packet_junk_size | 65535 | 65535 - 92 = **65443** |
| s2 | response_packet_junk_size | 65535 | 65535 - 60 = **65475** |
| s3 | cookie_reply_packet_junk_size | 65535 | 65535 - 56 = **65479** |
| s4 | transport_packet_junk_size | 65535 | 65535 - 32 = **65503** |

Source: `device.c:584-602`

The kernel validates: `junk_size + message_fixed_size <= MESSAGE_MAX_SIZE (65535)`

If too large, kernel returns:
```
<HWNAME>: S1 is too large
<HWNAME>: S2 is too large
<HWNAME>: S3 is too large
<HWNAME>: S4 is too large
```

**UI Note:** GtkAdjustment for S3/S4 should ideally be limited to 65479/65503, but the kernel will return an error if exceeded.

#### I1-I5: Init Packet Contents (CPS Format)

Format: tag-based string in CPS (Custom Protocol Signature) format parsed by `jp_parse_tags()` in `junk.c`.

**Syntax:**
```
i{n} = <tag1><tag2><tag3>...
```

Tags are separated by `<` and `>`. Space between tag and value is required (except for `<t>` and `<c>`).

**Available tags:**

| Tag | Format | Description | Constraints | Example |
|-----|--------|-------------|-------------|---------|
| `b` | `<b 0xHEX>` | Static bytes for protocol imitation | Arbitrary length, even hex digits | `<b 0xDEADBEEF>` |
| `c` | `<c>` | Packet counter (32-bit, network byte order) | No value allowed | `<c>` |
| `t` | `<t>` | Unix timestamp (32-bit, network byte order) | No value allowed | `<t>` |
| `r` | `<r N>` | Cryptographically secure random bytes | N ≤ 65535 | `<r 16>` |
| `rc` | `<rc N>` | Random ASCII alphanumeric `[A-Za-z0-9]` | N ≤ 65535 | `<rc 10>` → "aB3dEf9H2k" |
| `rd` | `<rd N>` | Random decimal digits `[0-9]` | N ≤ 65535 | `<rd 5>` → "13654" |

**Important:** Space after tag letter is required:
- `<r 16>` ✓ (valid)
- `<r16>` ✗ (invalid)

For tags `t` and `c`, no value allowed:
- `<t>` ✓ (valid)
- `<t 0>` ✗ (invalid)

**Examples:**
```
<b 0xDEADBEEF><rc 8><t><r 50>  → example CPS packet
<r 16>                             → 16 random bytes
<b 0xDEADBEEF>                    → 4 bytes: 0xDE, 0xAD, 0xBE, 0xEF
<b 0xAABBCC><r 16><t>            → 4 bytes + 16 random + timestamp
```

**Validation:** `jp_spec_setup()` in `junk.c` (lines 244-318):
- Total packet size must be <= MESSAGE_MAX_SIZE (65535)
- Invalid tags return -EINVAL

If invalid, kernel returns:
```
<HWNAME>: I1-packet invalid format
<HWNAME>: I2-packet invalid format
...
```

**Memory management:** I1-I5 strings are duplicated with `kstrdup()` in kernel. The compiled packet data (`pkt`) and modifiers are freed with `jp_spec_free()`.

#### Kernel Validation Order

1. Individual parameters parsed via netlink (`netlink.c:wg_set_device`)
2. `wg_device_handle_post_config()` called AFTER all params set, BEFORE peers
3. If validation fails, entire transaction is rolled back

```c
// netlink.c:wg_set_device (line 914)
ret = wg_device_handle_post_config(wg);
if (ret < 0)
    goto out;
```

This means partial configuration is NOT possible - all params must be valid together.

- `shared/awg/awg-nm-connection.h/c` - NMConnection translation
  - `awg_device_new_from_nm_connection()` - restore from NMConnection
  - `awg_device_save_to_nm_connection()` - save to NMConnection
  - `awg_nm_connection_set_peer_preshared_key()` - write a peer PSK and its flags (see "Preshared Keys")
  - `awg_nm_connection_apply_edited_psk()` - applies the editor's PSK field and storage choice to a peer (see "Preshared Keys")
  - Uses NMSettingIP4Config/NMSettingIP6Config for addresses and DNS
  - Multiple peers supported via indexed keys, consecutive from 0, at most `NM_AWG_MAX_PEERS` (16)

- `shared/awg/awg-secrets.h/c` - which secrets a connection uses and which are missing
  - `awg_secrets_list()` - private key plus every configured peer PSK, with flags
  - `awg_secrets_has_preshared_key()` - whether a peer has a PSK at all; `awg_device_new_from_nm_connection()` reads PSK flags only then
  - `awg_secrets_find_missing()` / `awg_secret_is_missing()` - the NeedSecrets decision
  - `awg_secret_is_missing_stored()` - a missing secret with flags 0 (private key or PSK), the case that needs the remedies below
  - GLib + libnm only, so the auth dialog links it without the rest of `shared/awg`; it is part of the core library too

### VPN Data Keys (defined in awg-nm-connection.h)

**Interface (NM_AWG_VPN_CONFIG_DEVICE_*):**
| Constant | Key | Secret |
|----------|-----|--------|
| NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY | local-private-key | yes |
| NM_AWG_VPN_CONFIG_DEVICE_PUBLIC_KEY | peer-public-key | no |
| NM_AWG_VPN_CONFIG_DEVICE_LISTEN_PORT | local-listen-port | no |
| NM_AWG_VPN_CONFIG_DEVICE_MTU | interface-mtu | no |
| NM_AWG_VPN_CONFIG_DEVICE_FW_MARK | interface-fw-mark | no |
| NM_AWG_VPN_CONFIG_DEVICE_JC | connection-jc | no |
| NM_AWG_VPN_CONFIG_DEVICE_JMIN | connection-jmin | no |
| NM_AWG_VPN_CONFIG_DEVICE_JMAX | connection-jmax | no |
| NM_AWG_VPN_CONFIG_DEVICE_S1 | connection-s1 | no |
| NM_AWG_VPN_CONFIG_DEVICE_S2 | connection-s2 | no |
| NM_AWG_VPN_CONFIG_DEVICE_S3 | connection-s3 | no |
| NM_AWG_VPN_CONFIG_DEVICE_S4 | connection-s4 | no |
| NM_AWG_VPN_CONFIG_DEVICE_H1 | connection-h1 | no |
| NM_AWG_VPN_CONFIG_DEVICE_H2 | connection-h2 | no |
| NM_AWG_VPN_CONFIG_DEVICE_H3 | connection-h3 | no |
| NM_AWG_VPN_CONFIG_DEVICE_H4 | connection-h4 | no |
| NM_AWG_VPN_CONFIG_DEVICE_I1 | connection-i1 | no |
| NM_AWG_VPN_CONFIG_DEVICE_I2 | connection-i2 | no |
| NM_AWG_VPN_CONFIG_DEVICE_I3 | connection-i3 | no |
| NM_AWG_VPN_CONFIG_DEVICE_I4 | connection-i4 | no |
| NM_AWG_VPN_CONFIG_DEVICE_I5 | connection-i5 | no |
| NM_AWG_VPN_CONFIG_DEVICE_HEADER_PROTECTION_KEY | connection-header-protection-key | no |
| NM_AWG_VPN_CONFIG_DEVICE_CONTENT_PADDING_ADDITION | connection-content-padding-addition | no |
| NM_AWG_VPN_CONFIG_DEVICE_REKEY_AFTER_TIME | connection-rekey-after-time | no |
| NM_AWG_VPN_CONFIG_DEVICE_REKEY_TIMEOUT | connection-rekey-timeout | no |
| NM_AWG_VPN_CONFIG_DEVICE_REJECT_AFTER_TIME | connection-reject-after-time | no |
| NM_AWG_VPN_CONFIG_DEVICE_KEEPALIVE_TIMEOUT | connection-keepalive-timeout | no |
| NM_AWG_VPN_CONFIG_DEVICE_MAX_HANDSHAKE_ATTEMPTS | connection-max-handshake-attempts | no |
| NM_AWG_VPN_CONFIG_DEVICE_RANDOM_TRAILERS | connection-random-trailers | no |
| NM_AWG_VPN_CONFIG_DEVICE_DISABLE_COOKIES | connection-disable-cookies | no |

**Peer (NM_AWG_VPN_CONFIG_PEER_*, pattern with %d):**
| Constant | Key | Secret |
|----------|-----|--------|
| NM_AWG_VPN_CONFIG_PEER_PUBLIC_KEY | peer-%d-public-key | no |
| NM_AWG_VPN_CONFIG_PEER_PRESHARED_KEY | peer-%d-preshared-key | yes |
| NM_AWG_VPN_CONFIG_PEER_ALLOWED_IPS | peer-%d-allowed-ips | no |
| NM_AWG_VPN_CONFIG_PEER_ENDPOINT | peer-%d-endpoint | no |
| NM_AWG_VPN_CONFIG_PEER_KEEP_ALIVE | peer-%d-keep-alive | no |
| NM_AWG_VPN_CONFIG_PEER_ADVANCED_SECURITY | peer-%d-advanced-security | no |

## Editor Validation

The properties editor (`properties/nm-amneziawg-editor.c`) validates input fields before allowing save:

### Interface Fields
- **Private Key** (required): `awg_validate_key()`, base64 of exactly 32 bytes (what the key setters accept)
- **Listen Port** (optional): Valid port number (0-65535)
- **MTU** (optional): Value 0 means auto, 68-1500 for custom
- **FwMark** (optional): 32-bit unsigned integer
- **jc, jmin, jmax, s1-s4** (optional): AmneziaWG protocol parameters (uint16)
- **h1-h4** (optional): Magic header values - arbitrary strings, max length 5KB each
- **i1-i5** (optional): Init packet content - arbitrary strings, max length 5KB each, preserves whitespace

### Peer Fields (via dialog)
- **Public Key** (required): `awg_validate_key()`, base64 of exactly 32 bytes
- **Endpoint** (required): Must be valid format (host:port)
- **Allowed IPs** (optional): Comma-separated list of subnets
- **Preshared Key** (optional): empty, or a key `awg_validate_key()` accepts (checked unless "Ask every time"/"not required" is picked, which clear the field)
- **Keep-alive** (optional): 0-65535 seconds

### Empty Key Handling
Functions like `awg_key_from_base64()` and `awg_device_peer_set_shared_key()` handle NULL/empty strings gracefully - they clear the existing key rather than failing.

### Scripts (Pre/Post Up/Down)

The editor does not expose pre-up, post-up, pre-down, post-down script fields in the UI. These values are stored in the VPN configuration for compatibility with awg-quick but are not editable through the NetworkManager interface.

## Secrets Handling

The plugin uses NM's built-in secret agent system. Secrets are stored using `nm_setting_vpn_add_secret()` and retrieved via `nm_setting_vpn_get_secret()`.

There are two kinds of secrets: `local-private-key` (always used) and `peer-N-preshared-key` (only for peers that have one).

### Secret Flags

| Flag | Value | Behavior |
|------|-------|----------|
| none | 0 | System stores to disk in plaintext |
| agent-owned | 1 | Stored in user's keyring (gnome-keyring) |
| not-saved | 2 | Ask user for key each time |
| not-required | 4 | Key is optional |

### Implementation

- **Storage**: Use `nm_setting_vpn_add_secret()` for private keys and preshared keys
- **Retrieval**: Use `nm_setting_vpn_get_secret()` in `awg-nm-connection.c`
- **Flags**: Use `nm_setting_set_secret_flags()` / `nm_setting_get_secret_flags()`
- **Which secrets are needed**: only `awg_secrets_list()` (`shared/awg/awg-secrets.c`) decides. The service answers `NeedSecrets`/`NewSecrets` with it and the auth dialog builds its prompt from it, so the two cannot disagree. Do not add a second copy of the rules.

### Preshared Keys

Modelled on NetworkManager's own WireGuard peers (`nm_wireguard_peer_new()` defaults PSK flags to NOT_REQUIRED; `nmcli connection import type wireguard` sets flags 0 only for a `PresharedKey` line):

- `awg_device_peer_new()` starts with `NM_SETTING_SECRET_FLAG_NOT_REQUIRED`: no PSK. `awg-config.c` sets flags 0 when it parses a `PresharedKey`.
- `awg_nm_connection_set_peer_preshared_key()` writes nothing for a peer without a key and with NOT_REQUIRED, so its `vpn.data` has no `peer-N-preshared-key-flags`. Every other combination keeps its flags.
- `awg_secrets_has_preshared_key()` treats "neither a non-empty key nor flags" as "no PSK". An empty stored value counts as none: `nmcli --ask` sends one when its PSK prompt is answered empty, and NM adds it to the connection's secrets (nm-applet leaves empty fields out of its reply). Do not test for flags with `nm_setting_get_secret_flags()`: it also succeeds without a `-flags` item whenever a value, even an empty one, is stored. Flags present without a value means a key is expected.
- **Flags 0 without a value stay required**, as for NetworkManager's own WireGuard peers. Version 0.9.12 wrote `peer-N-preshared-key-flags=0` for every peer, on import and in the editor alike; that is indistinguishable from a stored PSK that went missing from `vpn.secrets` (for example after an editor saved other secrets, which replaces the stored ones). Treating it as optional would connect without the PSK, and WireGuard then drops the handshake without any error. Such profiles never connected with 0.9.12 either. So the key is asked for, and the remedy goes where users see it: the auth dialog adds it to the prompt's `Description` (shown by gnome-shell, nm-applet and nmcli, with or without `--ask`): "The password is not required" for that PSK in the editor, or `nmcli connection modify uuid <uuid> -vpn.data peer-N-preshared-key-flags`. `need_secrets()` logs the same at `<warn>`, which NM only passes on with `VPN_PLUGIN` logging enabled (otherwise it starts the service with `NM_VPN_LOG_LEVEL=0`).
- **A lost secret stored for all users (flags 0)**, private key or PSK, can be typed in again only by a user allowed to modify the connection: NM keeps system-owned secrets from an agent only if polkit grants its user `org.freedesktop.NetworkManager.settings.modify.own` for a profile whose `connection.permissions` name exactly one user, `...settings.modify.system` for any other (`nm-agent-manager.c`, `_con_get_request_start()`; `nm-settings-connection.c`), and for other agents it marks the existing ones NOT_REQUIRED (`nm-agent-manager.c`, `set_secrets_not_required()`). Otherwise the prompt is futile, so the auth dialog's `Description` and the `<warn>` log name the way out: `sudo nmcli connection modify uuid <uuid> +vpn.secrets <name>=KEY`. (`-vpn.secrets <name>` does not remove a secret in nmcli 1.58; switching the flags to 2 drops a stored one.)
- Peers are numbered from 0 in every user-visible text (prompt labels, remedy, log), as in their `peer-N-*` keys.
- Editor peer dialog (`awg_nm_connection_apply_edited_psk()`): the field goes through `awg_device_peer_set_shared_key()` first, and the flags follow what was stored. A key it rejects changes neither key nor flags (the dialog validates with `awg_validate_key()` before, so this does not happen through the UI); storing flags for a key that is not there would ask for it forever. An empty PSK field means "no PSK" when the peer had none or the key shown was cleared, whatever storage was picked, except "Ask for this password every time". A peer whose stored key the editor was not given (flags 0 or 1, no value) keeps the picked flags, so the key is still asked for. The field is validated with the flags chosen in the menu, not the old ones.

### VPN Plugin Interface

The service implements these methods from `NMVpnServicePlugin`:

- `need_secrets()` - Returns the `vpn` setting when `awg_secrets_find_missing()` finds a required secret without a value (logged at `<info>`, the remedies for a missing flags-0 secret at `<warn>`; both only with `VPN_PLUGIN` logging enabled)
- `new_secrets()` - Fails with `Required secret <name> is still missing` if one is still missing. Both go through `find_missing_secret()`, which also sets the error for a connection without a `vpn` setting
- `connect()` / `connect_interactive()` - Creates AWGDevice from NMConnection (secrets loaded automatically)

The service never emits `SecretsRequired`, so there are no hints and the `.name` file does not set `supports-hints`.

### Flow

1. User creates VPN connection with empty key + "Ask every time" flag
2. NM calls `need_secrets()` → returns TRUE
3. NM asks the registered secret agent (gnome-shell, nm-applet, `nmcli --ask`). The agent runs `nm-amneziawg-auth-dialog` (the `[GNOME] auth-dialog` key of `nm-amneziawg-service.name`) and draws the prompt it describes
4. NM calls `need_secrets()` again with the secrets the agent returned; while it still returns TRUE, NM goes on to the next pass and finally fails with `no-secrets`
5. NM calls `connect()` with connection containing the key
6. Plugin creates AWGDevice from connection (key now available)

`new_secrets()` is not part of this flow: NM calls `NewSecrets` only on the interactive pass that follows a `SecretsRequired` signal (`nm-vpn-connection.c`), and the service never emits one. It is implemented for the Failure Contract.

Passes NM makes for a VPN (`nm-vpn-connection.c`): system secrets only, then agents without interaction (`-i` not given), then agents with interaction. VPNs never get `REQUEST_NEW`. Editors (GNOME Settings, nm-connection-editor) call `GetSecrets` when they open a saved profile, which also goes through the agent.

### Auth Dialog (`src/nm-amneziawg-auth-dialog.c`)

A GTK-free helper (GLib + libnm), installed to `${libexecdir}` and built in every configuration. Modelled on `nm-openvpn-auth-dialog`. The `.name` file declares it with `supports-external-ui-mode=true`.

- **Invocation**: `-u UUID -n NAME -s SERVICE [-i] [-r] [-t HINT]... [--external-ui-mode]`. Reads `DATA_KEY=/DATA_VAL=/SECRET_KEY=/SECRET_VAL=` up to `DONE` with `nm_vpn_service_plugin_read_vpn_details()`.
- **External UI mode** (gnome-shell, nm-applet, nmcli): prints a key file. `[VPN Plugin UI]` comes first with `Version=2`, `Title` and `Description` (nm-applet and nmcli refuse the reply otherwise). Then one group per secret with `Value`, `Label`, `IsSecret=true`, `ShouldAsk`, `ForceEcho=false`. Secrets with nothing to hand back and nothing to ask are left out. Exits 0.
- **Old protocol** (agents without external UI mode): prints `name\nvalue\n` pairs, then two empty lines, then waits for `QUIT` on stdin (at most 20 s). The agent ends the request when the helper exits, so exiting early loses the reply. If a secret would have to be typed in, it prints nothing and exits 1: there is no window to ask with.
- **What is asked** (`should_ask()`): never without `-i`; never NOT_REQUIRED; always NOT_SAVED; everything with `-r`; otherwise only a missing secret. The value of a NOT_SAVED secret is neither handed back nor prefilled.
- **Description**: when a secret with flags 0 and no value is asked (`awg_secret_is_missing_stored()`), the standard sentence is followed by
  - for a PSK, the remedy for a peer without one: one `nmcli connection modify uuid <uuid> -vpn.data peer-N-preshared-key-flags` line per such key. An empty answer does not mean "no PSK" (gnome-shell keeps OK insensitive, nm-applet leaves the empty field out of its reply, nmcli sends the empty value; the key stays missing either way), so this is the way out users see;
  - for any of them, that NM saves a typed-in key stored for all users only for a user allowed to modify the connection, and one `sudo nmcli connection modify uuid <uuid> +vpn.secrets <name>=KEY` line per such key (see "Preshared Keys"). The helper cannot tell whether the agent has that permission, nor which one applies (it gets `vpn.data`, not `connection.permissions`), so it says "if you may modify this connection"; the service's `<warn>` names the polkit action (`awg_secrets_modify_action()`).
- A plain lookup without `-i` never fails because something is missing: the editor would lose the other stored secrets.
- `signal(SIGPIPE, SIG_IGN)` comes first. gnome-shell closes the read end of the helper's stderr, so any diagnostic would otherwise kill it before it answers.
- Do not copy openvpn's dummy `no-secret=true` entry: NM would store it as a system-owned secret in the profile.
- nm-applet shows at most three asked secrets in one dialog. Others are dropped with a warning.
- gnome-shell 51 throws on a missing `auth-dialog` path and never answers, so before this helper GNOME Settings waited out libnm's 25 s D-Bus timeout each time it opened a saved profile.

### Failure Contract

No silent failures: every failure path must produce a diagnosable error, never a NULL `GError` or a bare log line.

- **D-Bus virtuals**: any `NMVpnServicePlugin` method returning `FALSE` must set `*error`. A `FALSE` with NULL error makes libnm emit `g_dbus_method_invocation_take_error: assertion 'error != NULL'`, send no reply, and NM fails activation with `Unknown reason` (`NoReply: Remote peer disconnected`). Guard every `FALSE` path, including fallbacks from manager calls (`new_secrets()` with still-missing secrets, which names the secret, `disconnect()` when the manager fails error-less).
- **Pre-flight validation**: `connect_worker` rejects invalid devices via `awg_device_get_invalid_reason()` before launching any backend. A device without PrivateKey (lost `vpn.secrets`) or peers otherwise produces a keyless config that `awg setconf`/awg-quick rejects with `Configuration parsing error`. Name the missing piece (`PrivateKey is missing…`, `Peer N: …`) so the log is actionable.
- **Backend exit codes**: the external manager reports non-zero `awg-quick` status via `g_spawn_check_exit_status()` instead of failing without an error. A failed `set_config()` signals `CONNECT_FAILED` instead of stalling NM into a timeout.

### UI Integration

The properties editor includes combo boxes for selecting secret flags:
- Private key has `interface_private_key_flags` combo
- Peer preshared key has `peer_psk_flags` combo

Combo options: "Store encrypted", "Ask every time", "Not required"

### Peer Editing Flow

The editor uses a TreeStore with peer pointers for efficient peer management:

```c
// TreeStore structure: (label, peer_pointer)
store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
```

**Key principles:**
1. **Clone for editing**: When opening the dialog, the peer is cloned (`awg_device_peer_new_clone()`)
2. **Cancel = no changes**: Original peer remains untouched; clone is discarded
3. **Apply = commit changes**: Clone data is copied back to original peer or added as new

**Dialog state management** (`AmneziaWGEditorPrivate`):
```c
AWGDevicePeer *dialog_peer;    // Currently editing peer (clone)
gint dialog_peer_index;        // -1 for new peer, >= 0 for existing
```

**Workflow:**
- **Add**: Create empty peer, clone it for dialog, set `dialog_peer_index = -1`
- **Edit**: Clone existing peer, set `dialog_peer_index` to its index
- **Cancel**: Simply close dialog, clone is discarded automatically
- **Apply**: Copy dialog data to peer, add/replace in device, rebuild TreeView

**Benefits:**
- No need to manage temporary peers in AWGDevice
- Clean separation between UI state and data model
- Simple cancel handling (just close and discard clone)

**Peer label priority:**
1. `endpoint - public_key...` (both present)
2. `endpoint` (only endpoint)
3. `public_key` (only public key)
4. `(new peer)` (empty peer)

## VPN Plugin IP Configuration

When implementing VPN plugins, it's crucial to properly configure IP settings in NetworkManager. The plugin must provide gateway and route information in the `set_config()` function.

### Required Configuration

**Gateway** - The VPN server address (from peer endpoint):
- Use `NM_VPN_PLUGIN_CONFIG_EXT_GATEWAY` key in the config builder
- Extract IP from peer endpoint (e.g., "203.0.113.1:51820" → "203.0.113.1")

**Routes** - VPN routes (from AllowedIPs):
- Use `NM_VPN_PLUGIN_IP4_CONFIG_ROUTES` for IPv4 routes
- Use `NM_VPN_PLUGIN_IP6_CONFIG_ROUTES` for IPv6 routes

### Using nm-utils for Routes

Prefer using NetworkManager's utility functions for route creation:

```c
#include <NetworkManager.h>

// Create routes array
GPtrArray *routes4 = g_ptr_array_new_full(10, (GDestroyNotify)nm_ip_route_unref);
GPtrArray *routes6 = g_ptr_array_new_full(10, (GDestroyNotify)nm_ip_route_unref);

// Parse AllowedIPs (e.g., "10.0.0.0/24, 0.0.0.0/0")
char **ip_list = g_strsplit(allowed_ips, ",", -1);
for (int i = 0; ip_list[i]; i++) {
    char *ip_str = g_strstrip(ip_list[i]);
    gboolean is_ipv6 = (strchr(ip_str, ':') != NULL);
    
    gchar **ip_parts = g_strsplit(ip_str, "/", 2);
    if (ip_parts[0] && ip_parts[1]) {
        guint prefix = (guint)g_ascii_strtoull(ip_parts[1], NULL, 10);
        GError *error = NULL;
        
        NMIPRoute *route = nm_ip_route_new(is_ipv6 ? AF_INET6 : AF_INET,
                                           ip_parts[0], prefix, NULL, -1, &error);
        if (route) {
            if (is_ipv6)
                g_ptr_array_add(routes6, route);
            else
                g_ptr_array_add(routes4, route);
        }
    }
    g_strfreev(ip_parts);
}
g_strfreev(ip_list);

// Convert to GVariant and add to config
if (routes4->len > 0) {
    GVariant *routes = nm_utils_ip4_routes_to_variant(routes4);
    g_variant_builder_add(&ip4builder, "{sv}", NM_VPN_PLUGIN_IP4_CONFIG_ROUTES, routes);
}
g_ptr_array_unref(routes4);

if (routes6->len > 0) {
    GVariant *routes = nm_utils_ip6_routes_to_variant(routes6);
    g_variant_builder_add(&ip6builder, "{sv}", NM_VPN_PLUGIN_IP6_CONFIG_ROUTES, routes);
}
g_ptr_array_unref(routes6);
```

### Important Keys

From `/usr/include/libnm/nm-vpn-dbus-interface.h`:

| Key | Description |
|-----|-------------|
| `NM_VPN_PLUGIN_CONFIG_EXT_GATEWAY` | External VPN gateway (IP address) |
| `NM_VPN_PLUGIN_CONFIG_HAS_IP4` | Whether IPv4 config exists |
| `NM_VPN_PLUGIN_CONFIG_HAS_IP6` | Whether IPv6 config exists |
| `NM_VPN_PLUGIN_IP4_CONFIG_ROUTES` | IPv4 routes array |
| `NM_VPN_PLUGIN_IP6_CONFIG_ROUTES` | IPv6 routes array |
| `NM_VPN_PLUGIN_IP4_CONFIG_NEVER_DEFAULT` | Prevent VPN from being default route |

### Common Errors

- **"no VPN gateway address received"** - Gateway not set in config
- **Crashes on route processing** - Memory management issues with NMIPRoute arrays

### Dependencies (allowed in src/)

**Allowed:**
- GLib (`<glib.h>`, `<gio/gio.h>`)
- NetworkManager types (`<NetworkManager.h>`)
- nm-utils functions (`nm_utils_ip4_routes_to_variant()`, etc.)

**NOT allowed in src/:**
- `nm-default.h`
- `nm-service-defines.h`

### Dependencies (allowed in shared/awg/)

**Allowed:**
- GLib (`<glib.h>`, `<gio/gio.h>`)
- NetworkManager types (`<NetworkManager.h>`)
- Local headers (`awg-device.h`, `awg-nm-connection.h`)

**NOT allowed:**
- `nm-default.h`
- `nm-service-defines.h`

### Testing

Tests for shared/awg/ can be run with:
```bash
cmake --build . --target nm-amneziawg-test
./build/nm-amneziawg-test
```

Note: `awg-nm-connection.c` requires running NetworkManager for full testing.

## Connection Manager Module (awg-connection-manager)

Module for managing AmneziaWG network connections. Provides an abstraction over different interface management methods.

### Architecture
```
AWGDevice -> AWGConnectionManager -> (netlink | external | dummy)
```

### Files

- `shared/awg/awg-connection-manager.h/c` - base interface
- `shared/awg/awg-connection-manager-dummy.h/c` - stub (always unavailable)
- `shared/awg/awg-connection-manager-external.h/c` - management via external awg-quick utility
- `shared/awg/awg-connection-manager-netlink.h/c` - direct management via netlink

### AWGConnectionManager Interface

```c
struct _AWGConnectionManagerInterface {
    GTypeInterface parent_iface;

    gboolean (*connect)(AWGConnectionManager *self, GCancellable *cancellable, GError **error);
    gboolean (*disconnect)(AWGConnectionManager *self, GError **error);
    gboolean (*manages_routes)(AWGConnectionManager *self);
};
```

### Constructor Functions

- `awg_connection_manager_dummy_new(interface_name, device)` - create dummy manager
- `awg_connection_manager_external_new(interface_name, device)` - create external manager
- `awg_connection_manager_netlink_new(interface_name, device)` - create netlink manager
- `awg_connection_manager_auto_new(interface_name, device)` - automatic selection (see below)

### Availability Check Functions

- `awg_connection_manager_dummy_is_available()` - always returns TRUE
- `awg_connection_manager_external_is_available()` - checks for awg-quick presence (`NM_AWG_QUICK_PATH`, else `/usr/local/sbin`, `/usr/local/bin`, `/usr/sbin`, `/usr/bin`, `/sbin`, `/bin`; not `PATH`)
- `awg_connection_manager_netlink_is_available()` - loads the `amneziawg` kernel module if needed (`modprobe`) and reports whether it is present

### Route Management

The `manages_routes()` method indicates whether the connection manager handles routes itself:

| Manager | Manages Routes | Notes |
|---------|----------------|-------|
| **netlink** | FALSE | NetworkManager adds routes based on AllowedIPs |
| **external** | TRUE | awg-quick handles routes internally |
| **dummy** | FALSE | Not functional |

**Why this matters:** If the manager already handles routes (external/awg-quick), NetworkManager should NOT add duplicate routes. The VPN plugin checks `manages_routes()` and skips route configuration when it returns TRUE.

### Automatic Selection (awg_connection_manager_auto_new)

Selection order:
1. If `NM_FORCE_AWG_QUICK` environment variable is set - tries external
2. Tries netlink
3. Tries external
4. Returns dummy as fallback

### Environment Variables

- `NM_AWG_QUICK_PATH` - path to awg-quick (for external implementation)
- `NM_FORCE_AWG_QUICK` - force using external implementation

### External Implementation (awg-quick)

- The configuration is generated from the `AWGDevice` (`awg_device_save_to_file()`) as `/run/nm-amneziawg/<ifname>.conf` (directory 0700, file 0400; awg-quick takes the interface name from the file name), right before `awg-quick up` and again before `awg-quick down` (which re-reads the hooks), and removed after each run. The `config-dir` construct property overrides the directory; only the tests use it.
- Why `/run`: NetworkManager.service has `PrivateTmp=true` and `KillMode=process`. When NetworkManager restarts with the tunnel up, the service (and `amneziawg-go`) outlive it, notice it gone (`NM_VPN_SERVICE_PLUGIN_DBUS_WATCH_PEER`) and disconnect, by then without the old private `/tmp`: with the file there, `awg-quick down` failed with `'/tmp/awg-….conf' does not exist`, the interface stayed, and every later activation failed with `'awg-…' already exists`.
- An interface that already has the connection's name when connecting is left over from an activation whose teardown never ran (the service was killed) or failed: `connect()` runs `awg-quick down` on it with the freshly generated file first (a failure there is only logged), then `awg-quick up`. The netlink backend does not do this; it tears down without files, so a NetworkManager restart does not leave interfaces behind.

### Netlink Implementation

**IMPORTANT: wgdevice_attribute enum**

The `wgdevice_attribute` enum in `shared/amneziawg.h` must match the kernel enum exactly. Missing attributes cause incorrect netlink messages.

Critical value: `WGDEVICE_A_PEER = 18` (between WGDEVICE_A_H4 and WGDEVICE_A_S3). Missing this causes all subsequent attribute types (S3, S4, I1-I5 and the AWG 3.1 attributes) to be off by one.

Reference: `amneziawg-linux-kernel-module` `src/uapi/wireguard.h` (tag `v3.1.20260812`, `WGDEVICE_A_MAX = __WGDEVICE_A_LAST - 1` at line 217) and `amneziawg-tools` `src/containers.h`

Uses built-in mini-libmnl from `shared/amneziawg.c` for:
- Creating/removing interface via genetlink
- Configuring WireGuard device via `wg_set_device()`
- Adding IP addresses via RTM_NEWADDR
- Bringing interface up/down via RTM_SETLINK

Supports all AmneziaWG parameters: jc, jmin, jmax, s1-s4, h1-h4, i1-i5, plus the AWG 3.1 set (header protection key, content padding addition, rekey/reject/keepalive ranges, max handshake attempts, random trailers, disable cookies)

**IMPORTANT:** `WG_GENL_VERSION` is **not** a compatibility gate. The 3.x module declares version 3 (`amneziawg-linux-kernel-module/src/uapi/wireguard.h:161`), the kernel stamps that value on replies only and never validates the version of an incoming `genlmsghdr`, and the vendored mini-libmnl in `shared/amneziawg.c` does not compare it either. What matters is attribute compatibility, and that is derived from the module *release* version (`/sys/module/amneziawg/version`) — see "AmneziaWG 3.1 Attributes" below.

#### AmneziaWG 3.1 Attributes (module 3.0+, flags 3.1+)

`WGDEVICE_A_HEADER_PROTECTION_KEY` … `WGDEVICE_A_DISABLE_COOKIES` are the last nine entries of the kernel's `wgdevice_attribute` enum. The first seven (`HEADER_PROTECTION_KEY` … `MAX_HANDSHAKE_ATTEMPTS`) appeared in module release 3.0 (`v3.0.20260730`), `RANDOM_TRAILERS` and `DISABLE_COOKIES` only in 3.1 (`v3.1.20260812`); the last 1.x release (`v1.0.20260725`) ends the enum at `WGDEVICE_A_I5` and encodes H1-H4 as `NLA_NUL_STRING` instead of packed `NLA_U64` ranges. amneziawg-tools gained the matching `.conf` keys in the same two steps (`src/config.c`: the seven in `v3.0.20260730`, `RandomTrailers`/`DisableCookies` in `v3.1.20260812`).

| Parameter (`.conf`) | Netlink attribute | Encoding |
|---|---|---|
| `HeaderProtectionKey` | `WGDEVICE_A_HEADER_PROTECTION_KEY` | 32 raw bytes (kernel policy `NLA_POLICY_EXACT_LEN(32)`) |
| `ContentPaddingAddition`, `RekeyAfterTime`, `RekeyTimeout`, `RejectAfterTime`, `KeepaliveTimeout`, `MaxHandshakeAttempts` | matching `WGDEVICE_A_*` | `u16_range_t` packed as `hi<<16 \| lo`, transmitted as `u32` |
| `RandomTrailers`, `DisableCookies` | `WGDEVICE_A_RANDOM_TRAILERS`, `WGDEVICE_A_DISABLE_COOKIES` | `u8` flag, only sent when enabled (`= on`) |

Generic netlink validates attributes strictly (`NL_VALIDATE_STRICT`, `lib/nlattr.c`), so a type above the module's `maxattr` fails the whole `WG_CMD_SET_DEVICE` request with `-EINVAL` / "Unknown attribute type". The netlink backend therefore asks `amneziawg_kernel_version()` first and refuses the connection with an actionable message naming the parameter and the release it needs (`awg_connection_manager_netlink_check_module()` in `awg-connection-manager-netlink.c`) instead of surfacing a bare `EINVAL`. The message must not suggest `NM_FORCE_AWG_QUICK=1`: awg-quick creates the interface with `ip link add … type amneziawg`, i.e. through the same module (the kernel loads an installed one on demand, `MODULE_ALIAS_RTNL_LINK`), and falls back to `amneziawg-go` only when no module can be loaded (`add_if()` in `wg-quick/linux.bash`). The way out is a newer module, loaded (reboot, or `modprobe -r amneziawg` with no interface up). `awg_device_get_required_awg_version()` decides per parameter: 3.1 for an enabled `RandomTrailers`/`DisableCookies`, 3.0 for the other seven, nothing otherwise. When the version cannot be read the attributes are still sent and the kernel decides, as before.

The external (`awg-quick`) backend has the same requirement on the tools side: the parser (`awg setconf`) rejects unknown keys with `Line unrecognized`. The backend checks the same required version against `awg --version` before writing anything, on the `awg` that `awg-quick` itself calls (found in `PATH`; awg-quick is found through the fixed list above) (`external_check_awg31_support()` in `awg-connection-manager-external.c`), and refuses with an actionable message. An unreadable or unparseable version is not blocked, the command then reports its own failure. Before the tools it runs the module check above (`awg_connection_manager_netlink_check_module()`), since awg-quick uses the module as well. A module that is installed but not loaded yet (`NM_FORCE_AWG_QUICK=1` skips the netlink probe that would load it) is loaded first with `modprobe amneziawg` from `PATH` (`external_load_module()`), because `ip link add` in awg-quick would load it anyway; skipping the check there let the first activation after boot reach `awg setconf`, which tools 3.x accept, and fail with `EINVAL` from the module. Only when no module can be loaded (awg-quick then uses `amneziawg-go`) is there no version, and the tools decide. Nothing is loaded for a configuration without 3.x parameters. The module version file is the construct-only property `module-version-file` (`AMNEZIAWG_MODULE_VERSION_FILE` by default), and the tests put a stub `modprobe` first in `PATH`, so they do not depend on the module of the host.

### Extended wg_device/wg_peer Structures (shared/amneziawg.h)

AmneziaWG fields added to `wg_device`:
- `junk_packet_count`, `junk_packet_min_size`, `junk_packet_max_size`
- `init_packet_junk_size`, `response_packet_junk_size`, `cookie_reply_packet_junk_size`, `transport_packet_junk_size` (s1-s4)
- `init_packet_magic_header`, `response_packet_magic_header`, `underload_packet_magic_header`, `transport_packet_magic_header` (h1-h4)
- `i1`, `i2`, `i3`, `i4`, `i5` - init packet contents (strings)
- `header_protection_key` (32 bytes), `content_padding_addition`, `rekey_after_time`, `rekey_timeout`, `reject_after_time`, `keepalive_timeout`, `max_handshake_attempts` (packed u16 ranges), `random_trailers`, `disable_cookies` (u8 flags) - AWG 3.1

### Integration with nm-amneziawg-service

Current implementation in `src/nm-amneziawg-service.c` creates the backend through `awg_connection_manager_auto_new()` and keeps it in the connection state; route add/delete is delegated to the backend when it `manages_routes()`.

## Editor Implementation

### GTK3/GTK4 Compatibility

The editor uses compatibility macros in `properties/gtk-compat.h`:

| GTK3 | GTK4 | Notes |
|------|------|-------|
| `gtk_entry_get_text()` | `AWG_EDITABLE_GET_TEXT()` | Use macro |
| `gtk_widget_get_toplevel()` | `gtk_widget_get_root()` | Returns `GtkRoot*` |
| `gtk_widget_destroy()` | `gtk_window_destroy()` | For dialogs |
| `gtk_grid_attach()` | Same | Grid unchanged |
| `gtk_box_pack_start()` | Same | Box unchanged |
| `gtk_label_new()` | Same | Labels unchanged |
| `gtk_spin_button_get_value()` | Same | SpinButtons unchanged |
| `gtk_toggle_button_get/set_active()` | `AWG_CHECK_BUTTON_GET/SET_ACTIVE()` | Use macro for every GtkCheckButton |

**GTK3 source, GTK4 copy generated at build time** (same scheme as
NetworkManager-openvpn): `properties/nm-amneziawg-dialog.ui` is a GTK3 file and the
only one to edit. With `WITH_GTK4=ON` the build converts it with
`scripts/gtk3to4-ui.sh` (`gtk4-builder-tool simplify --3to4`) into
`<build>/gtk4/nm-amneziawg-dialog.ui`. Each editor module embeds its own GResource
(`properties/resources.c` for GTK3, `gtk4/resources.c` for GTK4); the core library
carries none. The `.pot` is extracted from the source file only.

Rules for the source `.ui`:
- every widget that must be shown needs `visible=True`: `--3to4` turns a missing
  `visible` into `visible=0`, matching GTK3 semantics
- no `GtkButtonBox`, `GtkDialog.type-hint` or `<packing>` on GtkDialog/GtkInfoBar
  internal children — `--3to4` cannot convert them, and the script fails the build
  if any survive (Glade writes them back when it re-saves a GtkDialog)
- `can-focus`, `can-default` and `shadow-type` are handled by the conversion

`validate` passing is not enough: visibility and focus problems only show up in a
running editor, so check both GTK versions after layout changes.

Build dependency for `WITH_GTK4=ON`: `gtk4-builder-tool` (Fedora `gtk4-devel`,
Debian/Ubuntu `libgtk-4-bin`, Arch `gtk4`). GTK3-only and minimal builds don't need it.

**GTK4-specific:**
- GtkCheckButton is not a GtkToggleButton: use `AWG_CHECK_BUTTON_GET/SET_ACTIVE()`
- GtkSpinButton is not a GtkEntry: pass the widget to `AWG_EDITABLE_GET_TEXT()` without
  a `GTK_ENTRY()` cast
- Use `gtk_widget_get_root()` which returns `GtkRoot*`, cast to `GtkWindow*` for dialogs

**Editor module selection:** the descriptor plugin
(`properties/nm-amneziawg-editor-plugin.c:get_editor()`) chooses between
`libnm-vpn-plugin-amneziawg-editor.so` (GTK3) and
`libnm-gtk4-vpn-plugin-amneziawg-editor.so` (GTK4) at runtime by probing the calling
process for the GTK3-only symbol `gtk_container_add`. The relative module name is
resolved by `nm_vpn_plugin_utils_get_editor_module_path()` (dladdr) against the
directory of `libnm-vpn-plugin-amneziawg-core.so`, which contains that function, so
both editor modules must be installed next to the core library in `NM_PLUGIN_DIR`
(where the descriptor lives too).

### Numeric Fields

All numeric interface fields use `GtkSpinButton` with individual `GtkAdjustment`:

| Field | Adjustment Range |
|-------|------------------|
| Listen Port | 0 - 65535 |
| MTU | 0 (auto), 68-1500 |
| FwMark | 0 - 4294967295 (UINT32_MAX) |
| jc | 0 - 65535 |
| jmin | 0 - 65535 |
| jmax | 0 - 65535 |
| s1 | 0 - 65535 |
| s2 | 0 - 65535 |
| s3 | 0 - 65479 (kernel limit) |
| s4 | 0 - 65503 (kernel limit) |
| h1-h4 | string (max 5KB) |
| i1-i5 | string (max 5KB) |
| Keep-alive | 0 - 65535 |

**Important:** MTU = 0 means auto-detect. Each H1-H4 and JMin/JMax/S1/S2/S3/S4 field has its own adjustment (not shared).

### AmneziaWG 3.1 Fields

The nine AWG 3.1 parameters live in the `AmneziaWG 3.1` frame of `properties/nm-amneziawg-dialog.ui` (next to the 2.0 obfuscation groups). The key and the six timing/padding values are `GtkEntry` (they accept ranges like `115-125`, which a spin button cannot express), the two flags are `GtkCheckButton`.

| Widget | `vpn.data` key | Validation |
|---|---|---|
| `interface_header_protection_key_entry` | `connection-header-protection-key` | base64, exactly 32 decoded bytes |
| `interface_content_padding_addition_entry` | `connection-content-padding-addition` | `awg_range_parse_u32()` |
| `interface_rekey_after_time_entry` | `connection-rekey-after-time` | `awg_range_parse_u32()` |
| `interface_rekey_timeout_entry` | `connection-rekey-timeout` | `awg_range_parse_u32()` |
| `interface_reject_after_time_entry` | `connection-reject-after-time` | `awg_range_parse_u32()` |
| `interface_keepalive_timeout_entry` | `connection-keepalive-timeout` | `awg_range_parse_u32()` |
| `interface_max_handshake_attempts_entry` | `connection-max-handshake-attempts` | `awg_range_parse_u32()` |
| `interface_random_trailers_check` | `connection-random-trailers` | none, written as `on`/`off` |
| `interface_disable_cookies_check` | `connection-disable-cookies` | none, written as `on`/`off` |

An empty entry means "not set": the key is removed from `vpn.data`, and a disabled switch removes its key too, so a profile that does not use 3.1 parameters stays free of them. Rejected values keep the `error` style class on the field and name the offending key in the returned `GError`.

### Secret Field Handling

Use `nma_utils` functions for password/secret fields:
- `nma_utils_setup_password_storage()` - Initialize password field
- `nma_utils_update_password_storage()` - Update password on changes
- `nma_utils_menu_to_secret_flags()` - Get flags from menu selection

These provide popup menu for secret flags (agent-owned, always ask, not required).

### Validation

Use `awg/awg-validate.h` functions for validation:
- `awg_validate_base64()` - Validate base64 key
- `awg_validate_endpoint()` - Validate endpoint format (host:port)
- `awg_validate_mtu()` - Validate MTU (0 for auto, 68-1500)
- `awg_validate_port()` - Validate port number
- `awg_validate_fw_mark()` - Validate fwmark value
- `awg_validate_i_packet()` - Validate i1-i5 format (tags: r, rc, rd, c, t, b0x)
- `awg_validate_s3()` - Validate s3 (0-65479)
- `awg_validate_s4()` - Validate s4 (0-65503)
- `awg_validate_junk_size()` - Validate jc, jmin, jmax, s1, s2
- `awg_validate_magic_header()` - Validate magic header format (single number or range)
- `awg_validate_magic_headers_no_overlap()` - Validate that H1-H4 ranges don't overlap
- `awg_validate_allowed_ips()` - Validate allowed IPs subnets
- `awg_magic_header_parse()` - Parse magic header string into start/end values

### H1-H4 Range Validation

The editor validates H1-H4 magic headers at two levels:

1. **Individual field validation** (`check_interface_magic_header`):
   - Validates each field is either a single number or `start-end` range
   - Ensures start <= end in range format (e.g., "10-5" is invalid)

2. **Overlap validation** (`awg_validate_magic_headers_no_overlap`):
   - Called after individual field validation
   - Checks that no two non-empty ranges overlap
   - Visual feedback: marks all H1-H4 fields with "error" class on failure

### Common Issues

#### Crash when editing existing connections

**Problem:** NetworkManager crashes when validating IP settings in VPN editor.

**Cause:** `nm_connection_supports_ip4()` returns TRUE for all non-slave connections including VPNs. This causes validation to run on IP settings before they're properly configured.

**Solution:** Before validation runs, set IP4/IP6 methods to DISABLED/IGNORE:
```c
NMSettingIP4Config *s_ip4 = nm_connection_get_setting_ip4_config(connection);
g_object_set(s_ip4, NM_SETTING_IP4_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_DISABLED, NULL);
NMSettingIP6Config *s_ip6 = nm_connection_get_setting_ip6_config(connection);
g_object_set(s_ip6, NM_SETTING_IP6_CONFIG_METHOD, NM_SETTING_IP6_CONFIG_METHOD_IGNORE, NULL);
```

#### Empty key handling

Functions handle NULL/empty strings gracefully - they clear existing keys rather than failing:
- `awg_key_from_base64()` - Returns NULL for empty input
- `awg_device_peer_set_shared_key()` - Clears key if empty string passed

### UI Layout

The editor follows WireGuard-style layout:
1. **Interface section** - Private key, listen port, MTU, FwMark, AmneziaWG params (jc, jmin, jmax, s1, s2, s3, s4, h1-h4, i1-i5)
2. **Peers section** - TreeView with peer list, Add/Edit/Remove buttons, warning when no peers
3. **IP/DNS** - NOT in VPN editor, configured via standard NM IPv4/IPv6 tabs

Peer editing uses a separate dialog (not inline):
- Public key (required, validated)
- Endpoint (required, validated)
- Allowed IPs (optional)
- Preshared key (optional, with flags)
- Keep-alive (optional)

### Libraries

- Libraries should NOT have version in filename (no SOVERSION)
- Use linker flags `-Wl,--no-undefined` to catch missing symbols at link time
