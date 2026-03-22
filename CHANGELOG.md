# Changelog

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
