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

// Test round-trip (write and read)
gboolean saved = awg_device_save_to_file(device, output_path);
g_assert_true(saved);
AWGDevice *device2 = awg_device_new_from_config(output_path);
g_assert_true(awg_device_is_valid(device2));
```

## Localization (i18n)

Translations are stored in `po/` directory with `.po` files. CMake automatically handles everything.

### Build Process
- `.po` files are auto-detected via `file(GLOB ...)` in CMakeLists.txt
- `.gmo` (binary machine object) files are generated from `.po` during build
- `.pot` template is generated using xgettext with `-k_` flag

### Marking Strings for Translation
In C source files, wrap user-visible strings with `_()`:
```c
g_string_append(error_msg, _("Public key is required and must be valid base64\n"));
```

In UI files (.ui), use `translatable="yes"` property:
```xml
<property name="tooltip-markup" translatable="yes">Base64-encoded 32-byte private key.</property>
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
├── scripts/          # Helper scripts (build.sh, check-style.sh)
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

### Build Script Usage
```bash
./scripts/build.sh              # Default build (GTK3 if available)
./scripts/build.sh --gtk4       # Build with GTK4
./scripts/build.sh --minimal    # Build without editor (service only)
./scripts/build.sh --clean      # Clean and rebuild
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
- AmneziaWG parameters (jc, jmin, jmax, s1-s2, h1-h4)
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
| NM_AWG_VPN_CONFIG_DEVICE_H1 | connection-h1 | no |
| NM_AWG_VPN_CONFIG_DEVICE_H2 | connection-h2 | no |
| NM_AWG_VPN_CONFIG_DEVICE_H3 | connection-h3 | no |
| NM_AWG_VPN_CONFIG_DEVICE_H4 | connection-h4 | no |

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
- **jc, jmin, jmax, s1, s2, h1-h4** (optional): AmneziaWG protocol parameters

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

### Peer Editing

Peers are edited via a dialog (PeerDialog):
- Click "Add" to create new peer and open dialog
- Double-click on peer in list to edit
- Dialog validates public key (required, base64) and endpoint (required, host:port)
- Apply saves to AWGDevice, Cancel removes newly created peer
- Peers displayed in TreeView with public key or endpoint as label

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
    const gchar *(*get_interface_name)(AWGConnectionManager *self);
    AWGDevice *(*get_device)(AWGConnectionManager *self);
    gboolean (*is_running)(AWGConnectionManager *self);
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

Uses built-in mini-libmnl from `shared/amneziawg.c` for:
- Creating/removing interface via genetlink
- Configuring WireGuard device via `wg_set_device()`
- Adding IP addresses via RTM_NEWADDR
- Bringing interface up/down via RTM_SETLINK

Supports all AmneziaWG parameters: jc, jmin, jmax, s1-s2, h1-h4

### Extended wg_device/wg_peer Structures (shared/amneziawg.h)

AmneziaWG fields added to `wg_device`:
- `junk_packet_count`, `junk_packet_min_size`, `junk_packet_max_size`
- `init_packet_junk_size`, `response_packet_junk_size`
- `init_packet_magic_header`, `response_packet_magic_header`, `underload_packet_magic_header`, `transport_packet_magic_header`

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
|-------|-------------------|
| Listen Port | 0 - 65535 |
| MTU | 0 (auto), 68-1500 |
| FwMark | 0 - 4294967295 (UINT32_MAX) |
| jc | 0 - 65535 |
| jmin | 0 - 65535 |
| jmax | 0 - 65535 |
| s1 | 0 - 65535 |
| s2 | 0 - 65535 |
| h1 | 0 - 4294967295 |
| h2 | 0 - 4294967295 |
| h3 | 0 - 4294967295 |
| h4 | 0 - 4294967295 |
| Keep-alive | 0 - 450 |

**Important:** MTU = 0 means auto-detect. Each H1-H4 and JMin/JMax/S1/S2 field has its own adjustment (not shared).

### Secret Field Handling

Use `nma_utils` functions for password/secret fields:
- `nma_utils_setup_password_storage()` - Initialize password field
- `nma_utils_update_password_storage()` - Update password on changes
- `nma_utils_menu_to_secret_flags()` - Get flags from menu selection

These provide popup menu for secret flags (agent-owned, always ask, not required).

### Validation

Use `awg/awg-validate.h` functions for validation:
- `awg_validate_key()` - Validate base64 key
- `awg_validate_key_any()` - Validate key or empty
- `awg_validate_endpoint()` - Validate endpoint format (host:port)
- `awg_validate_mtu()` - Validate MTU (0 for auto, 68-1500)
- `awg_validate_port()` - Validate port number
- `awg_validate_fwmark()` - Validate fwmark value

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
1. **Interface section** - Private key, listen port, MTU, FwMark, AmneziaWG params (jc, jmin, jmax, s1, s2, h1-h4)
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
