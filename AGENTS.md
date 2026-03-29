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
| `WITH_GTK3` | Build with GTK3 editor (requires libnma) | Auto-detect |
| `WITH_GTK4` | Build with GTK4 editor (requires libnma-gtk4) | Auto-detect |
| `CMAKE_INSTALL_PREFIX` | Install prefix | `/usr` |
| `CMAKE_INSTALL_LIBDIR` | Library directory (e.g., lib64) | Auto-detect |

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
cmake .. -DWITH_GTK3=ON -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib64
cmake --build .
cpack -G RPM

# Build DEB package (Debian/Ubuntu)
cmake .. -DWITH_GTK3=ON -DCMAKE_INSTALL_PREFIX=/usr
cmake --build .
cpack -G DEB
```

### Library Architecture

The project uses a shared library for common code:

```
libnm-vpn-plugin-amneziawg-core.so  ← AWGDevice, AWGConfig, etc.
    ↓                                    ↓
libnm-vpn-plugin-amneziawg.so     libnm-vpn-plugin-amneziawg-editor.so (GTK3/GTK4)
```

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
ctest -R amneziawg-test

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
| `awg_validate_base64()` | Validates base64-encoded key | 32 bytes decoded |
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
- Code style validation (clang-format)
- Build on Ubuntu (GTK3) + tests + DEB artifact
- Build on Fedora (GTK3) + tests + RPM artifact
- Minimal build (no GTK, service only)
- GTK4 build

**Release Packages** (`.github/workflows/release.yml`) - Runs on release:
- Builds RPM (Fedora) and DEB (Ubuntu) packages
- Uploads to GitHub Releases
- Can be triggered manually via workflow_dispatch

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

### Includes
- Keep includes minimal - only include what's actually used
- Remove unused system and local includes to improve compile times
- Group includes: system headers first, then project headers

## Directory Layout

```
├── src/              # Service binary (nm-amneziawg-service)
├── properties/       # Connection editor plugin (GTK3/GTK4)
├── shared/           # Shared utility code
│   ├── awg/          # AmneziaWG core module
│   ├── nm-utils/     # NetworkManager utilities
│   └── amneziawg.c   # Netlink helper
├── tests/            # Unit tests using GLib test framework
├── po/               # Translations
├── scripts/          # Helper scripts (build.sh, check-style.sh, fix-pot-paths.sh)
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

### Build Script Usage
```bash
./scripts/build.sh              # Default build (GTK3 if available)
./scripts/build.sh --gtk4       # Build with GTK4
./scripts/build.sh --minimal    # Build without editor (service only)
./scripts/build.sh --clean       # Clean and rebuild
```

### Style Check Usage
```bash
./scripts/check-style.sh        # Check all source files
./scripts/check-style.sh --fix  # Auto-fix style issues
```

## Runtime Requirements
- **amneziawg-tools** must be installed (external VPN tunnel dependency)
- D‑Bus policy file needs `/etc/dbus-1/system.d`
- Plugins installed to `/usr/lib/NetworkManager/VPN`

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
  - Uses NMSettingIP4Config/NMSettingIP6Config for addresses and DNS
  - Multiple peers supported via indexed keys

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
- **Private Key** (required): Must be valid base64-encoded 32-byte key
- **Listen Port** (optional): Valid port number (0-65535)
- **MTU** (optional): Value 0 means auto, 68-1500 for custom
- **FwMark** (optional): 32-bit unsigned integer
- **jc, jmin, jmax, s1-s4** (optional): AmneziaWG protocol parameters (uint16)
- **h1-h4** (optional): Magic header values - arbitrary strings, max length 5KB each
- **i1-i5** (optional): Init packet content - arbitrary strings, max length 5KB each, preserves whitespace

### Peer Fields (via dialog)
- **Public Key** (required): Must be valid base64-encoded 32-byte key
- **Endpoint** (required): Must be valid format (host:port)
- **Allowed IPs** (optional): Comma-separated list of subnets
- **Preshared Key** (optional): Must be valid base64 if provided
- **Keep-alive** (optional): 0-65535 seconds

### Empty Key Handling
Functions like `awg_key_from_base64()` and `awg_device_peer_set_shared_key()` handle NULL/empty strings gracefully - they clear the existing key rather than failing.

### Scripts (Pre/Post Up/Down)

The editor does not expose pre-up, post-up, pre-down, post-down script fields in the UI. These values are stored in the VPN configuration for compatibility with awg-quick but are not editable through the NetworkManager interface.

## Secrets Handling

The plugin uses NM's built-in secret agent system. Secrets are stored using `nm_setting_vpn_add_secret()` and retrieved via `nm_setting_vpn_get_secret()`.

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

### VPN Plugin Interface

The service implements these methods from `NMVpnServicePlugin`:

- `need_secrets()` - Checks if private key or any preshared key is missing (and not NOT_REQUIRED)
- `new_secrets()` - Validates updated secrets from NM before connection
- `connect()` / `connect_interactive()` - Creates AWGDevice from NMConnection (secrets loaded automatically)

### Flow

1. User creates VPN connection with empty key + "Ask every time" flag
2. NM calls `need_secrets()` → returns TRUE
3. NM's built-in secret agent prompts user for the key
4. NM calls `new_secrets()` with the provided key
5. NM calls `connect()` with connection containing the key
6. Plugin creates AWGDevice from connection (key now available)

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

    gboolean (*connect)(AWGConnectionManager *self, GError **error);
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
- `awg_connection_manager_external_is_available()` - checks for awg-quick presence
- `awg_connection_manager_netlink_is_available()` - checks for amneziawg/wireguard kernel module

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

### Netlink Implementation

**IMPORTANT: wgdevice_attribute enum**

The `wgdevice_attribute` enum in `shared/amneziawg.h` must match the kernel enum exactly. Missing attributes cause incorrect netlink messages.

Critical value: `WGDEVICE_A_PEER = 18` (between WGDEVICE_A_H4 and WGDEVICE_A_S3). Missing this causes all subsequent attribute types (S3, S4, I1-I5) to be off by one.

Reference: `/usr/src/amneziawg-1.0.20260210/uapi/wireguard.h` and `/home/vovochka/src/amneziawg-tools/src/containers.h`

Uses built-in mini-libmnl from `shared/amneziawg.c` for:
- Creating/removing interface via genetlink
- Configuring WireGuard device via `wg_set_device()`
- Adding IP addresses via RTM_NEWADDR
- Bringing interface up/down via RTM_SETLINK

Supports all AmneziaWG parameters: jc, jmin, jmax, s1-s4, h1-h4, i1-i5

**IMPORTANT:** `WG_GENL_VERSION` must be 2 to match the kernel module (see `/usr/src/amneziawg-1.0.20260210/uapi/wireguard.h:161`).

### Extended wg_device/wg_peer Structures (shared/amneziawg.h)

AmneziaWG fields added to `wg_device`:
- `junk_packet_count`, `junk_packet_min_size`, `junk_packet_max_size`
- `init_packet_junk_size`, `response_packet_junk_size`, `cookie_reply_packet_junk_size`, `transport_packet_junk_size` (s1-s4)
- `init_packet_magic_header`, `response_packet_magic_header`, `underload_packet_magic_header`, `transport_packet_magic_header` (h1-h4)
- `i1`, `i2`, `i3`, `i4`, `i5` - init packet contents (strings)

### Integration with nm-amneziawg-service

Current implementation in `src/nm-amneziawg-service.c` uses external awg-quick utility directly. Planned migration to use `AWGConnectionManager`.

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

**GTK4-specific:**
- No `wrap` property on GtkLabel
- GtkGrid does NOT support `expand` and `fill` (only GtkBox does)
- Use `gtk_widget_get_root()` which returns `GtkRoot*`, cast to `GtkWindow*` for dialogs

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
