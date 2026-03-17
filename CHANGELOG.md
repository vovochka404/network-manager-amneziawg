# Changelog

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
