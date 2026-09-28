# Changelog

## [Unreleased]

### Major Changes

#### AmneziaWG 3.1 Protocol Support
- **New 3.1 obfuscation parameters**: `HeaderProtectionKey`, `ContentPaddingAddition`, `RekeyAfterTime`, `RekeyTimeout`, `RejectAfterTime`, `KeepaliveTimeout`, `MaxHandshakeAttempts`, `RandomTrailers` and `DisableCookies` are now parsed from `.conf` files, stored on `AWGDevice`, serialised over `vpn.data` (`connection-*` keys) and passed to both backends. Previously a 3.1 server configuration could not be reproduced by the plugin, so clients were unable to complete a handshake against it
- **Both backends supported**: the external (`awg-quick`) path writes the parameters back into the generated configuration, and the netlink path sends the matching attributes (32-byte key, u16 ranges packed as `hi<<16 | lo` to match the kernel's `u16_range_t`, u8 flags)
- **Refused on kernels that do not know them**: the netlink backend checks the version the loaded module reports before encoding the 3.1 attributes and fails with an actionable message instead of letting an older module reject the whole request with `EINVAL` (`Unknown attribute type`). Everything from 3.0 on carries the attributes; when the version cannot be read the connection is still attempted and the fallback stays with the kernel. The version is now read in a single place, shared with the kernel ABI probe
- **Refused with tools that do not know them either**: the external backend runs `awg --version` on the binary `awg-quick` resolves from `PATH` before writing the configuration, and refuses a 3.1 profile when the tools are older than 3.0 (their parser aborts on the unknown keys with `Line unrecognized`)
- **Editable in the connection editor**: the nine parameters have fields in the GTK3/GTK4 editor (`AmneziaWG 3.1` frame — entries for the key and the six ranges, switches for the two flags), validated the same way the service validates them, so a 3.1 profile no longer has to be edited outside the GUI
- **The editor says which field it refuses**: the shared field check in `properties/nm-amneziawg-editor.c` never set its `GError` (the condition tested for a missing error instead of an existing one), so a rejected value produced a bare failure with only the red field to look at — the private key check was affected too. The new 3.1 fields ask for the message and report e.g. `ContentPaddingAddition`
- **Range validation matches the kernel encoding**: range values are accepted only when they fit the kernel representation, so a value that would be silently dropped on the netlink path is rejected up front. `HeaderProtectionKey` is checked for the exact 32-byte length the kernel requires (`NLA_POLICY_EXACT_LEN`)

## [0.9.11] - 2026-09-06

### Major Changes

#### Non-blocking Connection Setup
- **Async connection via GTask**: service `connect()` no longer blocks the main loop (previously the plugin froze after a failed handshake — the stalled loop left D-Bus `NeedSecrets` unanswered). The connection now runs in a `GTask` worker thread; a second `Connect` while one is in progress returns `WRONG_STATE`; a failed `set_config()` signals `CONNECT_FAILED` instead of stalling NM into a timeout. The connection manager `connect()` signature now takes a `GCancellable *`

#### AmneziaWG Kernel ABI Compatibility
- **Support old and current kernel ABIs**: magic headers (H1–H4) changed from NUL strings to packed u64 ranges and peer keepalive from u16 to u32 in newer kernels. The format is auto-detected from the loaded module version (3.x = new ABI), defaulting to the new ABI with legacy fallback when the version cannot be read

### Bug Fixes

#### VPN Plugin Factory Export
- **Fixed `nmcli connection import type amneziawg`**: `nm_vpn_editor_plugin_factory` was compiled only into the editor target where the version script hides it — the main plugin declared the symbol but never compiled the file, failing with "undefined symbol" on all platforms. The file is now part of the main plugin target, and editor-plugin linkage was moved out of the core library

#### Secrets & Config Handling
- **Fixed activation failure with `Unknown reason`**: `new_secrets()` could return `FALSE` without setting a `GError`, making libnm emit `g_dbus_method_invocation_take_error: assertion 'error != NULL'` so NM saw `Remote peer disconnected`. Every D-Bus virtual now sets `*error` on all `FALSE` paths, including fallbacks from manager calls
- **Pre-flight config validation**: `connect_worker` rejects invalid devices via the new `awg_device_get_invalid_reason()` before launching any backend. A keyless device (lost `vpn.secrets`) previously produced a config that `awg setconf`/awg-quick rejected with `Configuration parsing error`; the error now names the missing piece (`PrivateKey is missing…`, `Peer N: …`)
- **Report awg-quick failures**: the external manager converts a non-zero `awg-quick` exit status into a `GError` via `g_spawn_check_exit_status()` instead of failing silently

#### Netlink Robustness
- **Use absolute modprobe path** in `load_kernel_module()`
- **Copy input string before modifying** in `add_ip_address()` (was writing through a const pointer)
- **Use `strdup()` instead of `g_strdup()`** for `wg_device` string fields (freed with `free()`)
- **Check `inet_pton()` return value** when parsing Allowed IPs — malformed entries are skipped with a warning instead of producing garbage netlink messages
- **Propagate `add_ip_address()` failures** in the connect flow instead of bringing up an address-less interface
- **Return early when `if_nametoindex()` fails** instead of sending netlink messages with `ifi_index = 0`
- **Save `errno` before GLib type-check macros** in route functions (the macros may clobber it before `g_set_error()` reads it)

### Improvements

#### Documentation
- **Failure handling contract in AGENTS.md**: documents the D-Bus error contract, pre-flight validation, and backend exit-code reporting rules

### Testing

- **Added reproducing tests**: secrets round-trip through NMConnection, keyless-config detection, invalid-reason coverage, and force-quick backend selection with a hermetic awg-quick stub, plus a new `tests/test-config-no-keys.conf` fixture

---

## [0.9.9] — 2026-04-26

### Bug Fixes

- **Fixed VPN gateway for hostname endpoints**: VPN connections with hostname endpoints (e.g., `vpn.example.com:51820`) properly handled. Endpoint is resolved to IP address during connection and properly passed to NetworkManager as gateway

---

## [0.9.8] — 2026-04-23

### Major Changes

#### Connection Manager
- **Restored manual routes management**: Netlink backend now properly handles manual connections with user-defined routes via NetworkManager

### Bug Fixes

#### Connection Import
- **Fixed DNS import**: DNS servers are now correctly imported from config files to NMConnection settings

### Improvements

#### Connection Editor
- **Set connection name from filename on import**: When importing a config file, the connection now takes its name from the filename (e.g., `awg0.conf` → connection name `awg0`) instead of using a default name

---

## [0.9.7] — 2026-03-29

### Bug Fixes

#### Connection Editor

- **Fixed Allowed IPs display in peer editor**: Fixed critical bug where only the last subnet was displayed when editing peer with multiple Allowed IPs
- **Fixed subnet mask /0 handling**: Corrected parsing of subnet mask 0 (e.g., `0.0.0.0/0`)

### Improvements

#### Connection Editor

- **Added Allowed IPs validation**: Implemented validation for peer Allowed IPs field in editor dialog with visual error indication
- **Updated I1-I5 field tooltips**: Corrected documentation for init packet content fields with proper CPS tag format and examples

### Testing

- **Added peer clone test**: New test verifies correct cloning of peers with multiple Allowed IPs

---

## [0.9.6] — 2026-03-22

### Major Changes

#### AmneziaWG 2.0 Protocol Support
- Added support for new AmneziaWG obfuscation parameters (s3, s4, i1–i5)
- Extended configuration format with advanced security features
- Enhanced netlink implementation for full parameter synchronization

#### Connection Manager Improvements
- Refactored connection manager with improved backend selection
- Better integration between netlink and awg-quick implementations
- Enhanced route management with automatic backend detection

### Improvements

#### Editor UI Enhancements
- **Improved layout**: Grouped numeric parameters (Jc/JMin/JMax, S1–S4, H1–H4) into compact horizontal rows
- **Grid-based organization**: Magic headers and noise packet sizes now use 2×2 grid layout
- **Better spacing**: Consistent field widths and visual hierarchy across all sections
- **Peer management**: Added Edit button for easier peer configuration
- **Safety**: Delete confirmation dialog prevents accidental peer removal

#### Testing
- Extended test coverage for new AmneziaWG parameters
- Added netlink configuration tests
- Improved test fixtures with sample configuration files

#### Translations
- Updated translations for all supported languages (ru, de, en_GB, zh_CN)
- Added new strings for enhanced editor UI
- Fixed translation template generation

### Bug Fixes

- Fixed DNS configuration constant in service module
- Resolved editor hang on configuration export
- Corrected translation file paths in build scripts

### Cleanup

- Removed obsolete AUTHORS file
- Streamlined build configuration for better maintainability

---

## [0.9.5] — 2026-03-17

### Major Changes

#### Native AmneziaWG Support
- Implemented direct kernel module management via netlink for optimal performance
- Added awg-quick-based implementation as a fallback for systems without kernel support
- Automatic selection of available backend (netlink → awg-quick → dummy)
- Full support for AmneziaWG-specific parameters (jc, jmin, jmax, s1, s2, h1–h4)

#### New Configuration Architecture
- Introduced `AWGDevice` — a comprehensive GObject-based representation of WireGuard device configuration
- Added peer management with `AWGDevicePeer` and subnet handling with `AWGSubNet`
- Implemented native WireGuard config file parser/generator (`AWGConfig`)
- Created seamless NetworkManager integration layer (`AWGNMConnection`)
- Added validation utilities for all configuration parameters (`AWGValidate`)

#### Service & Editor Rewrite
- Completely rewrote the VPN service with simplified connection logic and better error handling
- Redesigned settings editor UI with improved validation and GTK3/GTK4 compatibility
- Enhanced peer management dialog with real-time validation

#### Build System Migration
- Migrated from autotools (autoconf/automake) to modern CMake build system
- Automatic detection and selection of GTK3/GTK4 based on system availability
- Integrated CPack for native DEB/RPM package generation
- Built-in translation handling with automatic `.gmo` file generation

### Improvements

- **CI/CD**: Added GitHub Actions workflows for continuous integration and automated releases
- **Testing**: Introduced GLib-based test framework with unit tests for core modules
- **Translations**: Updated and cleaned up translations (ru, de, en_GB, zh_CN), removed obsolete strings
- **Documentation**: Refreshed README with CMake instructions, added CONTRIBUTING guidelines

### Cleanup

- Removed legacy OpenVPN-related code and authentication dialogs
- Eliminated obsolete utility scripts and example files
- Cleaned up outdated test infrastructure and replaced with modern GLib-based tests
- Removed deprecated images and metadata files

---

**Commits:** 12 commits from `master..refactoring`
