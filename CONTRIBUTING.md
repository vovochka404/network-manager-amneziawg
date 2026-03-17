# Contributing to NetworkManager-amneziawg

Thank you for contributing to this project! This document provides guidelines and instructions for developers.

## Development Workflow

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Make your changes
4. Run tests and style checks locally
5. Commit your changes (see [Commit Guidelines](#commit-guidelines))
6. Push to your fork
7. Open a Pull Request

## Building from Source

### Quick Build

```bash
# Clone the repository
git clone https://github.com/amnezia-vpn/network-manager-amneziawg.git
cd network-manager-amneziawg

# Build with default options (GTK3 if available)
mkdir build && cd build
cmake ..
cmake --build .

# Run tests
ctest --output-on-failure

# Install (optional)
sudo cmake --install .
```

### Build Script

Use the provided build script for common tasks:

```bash
# Build with GTK3 (default)
./scripts/build.sh

# Build with GTK4
./scripts/build.sh --gtk4

# Minimal build (service only, no editor)
./scripts/build.sh --minimal

# Clean rebuild
./scripts/build.sh --clean
```

### Build Options

| CMake Option | Description | Default |
|--------------|-------------|---------|
| `WITH_GTK3` | Build GTK3 editor | Auto-detect |
| `WITH_GTK4` | Build GTK4 editor | Auto-detect |
| `CMAKE_INSTALL_PREFIX` | Install prefix | `/usr` |
| `CMAKE_INSTALL_LIBDIR` | Library directory | Auto-detect |

### Dependencies

#### Ubuntu/Debian

```bash
# Core dependencies
sudo apt-get install -y cmake gcc pkg-config \
    libnm-dev libglib2.0-dev

# GTK3 editor
sudo apt-get install -y libnma-dev gettext network-manager-dev

# GTK4 editor
sudo apt-get install -y libnma-gtk4-dev

# Package building
sudo apt-get install -y dpkg-dev debhelper rpm
```

#### Fedora

```bash
sudo dnf install -y cmake gcc pkg-config \
    NetworkManager-devel libnma-devel \
    glib2-devel gettext rpm-build
```

## Code Style

This project uses **clang-format** with GNU style (4-space indent, 80-column limit).

### Checking Code Style

```bash
# Check all source files
./scripts/check-style.sh

# Auto-fix style issues
./scripts/check-style.sh --fix
```

### Manual clang-format

```bash
# Check specific files
clang-format --style=file --dry-run --Werror src/file.c

# Fix specific files
clang-format --style=file -i src/file.c
```

### Style Guidelines

- **Formatting**: See `.clang-format` (GNU style, 4-space indent)
- **Braces**: K&R style, no braces after control statements
- **Naming**: 
  - Functions: `lowercase_with_underscores`
  - Types: `PascalCase` with `AWG` prefix
  - Constants: `UPPER_SNAKE_CASE`
- **Includes**: System headers first (angle brackets), then project headers (quotes)
- **Error handling**: GLib `GError` pattern, `g_warning()` for non-fatal

## Testing

### Running Tests

```bash
cd build

# Run all tests
ctest

# Run specific test
ctest -R amneziawg-test

# Verbose output
ctest --output-on-failure --verbose
```

### Writing Tests

Tests use GLib test framework. See `tests/test-main.c` for examples.

```c
#include <glib.h>
#include "awg/awg-device.h"

static void
test_example(void)
{
    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);
    // ... test logic ...
    g_object_unref(device);
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/awg/device/test", test_example);
    return g_test_run();
}
```

### Test Configuration Files

Place `.conf` test files in `tests/` directory. They are automatically copied to `build/tests/` during build.

## CI/CD

### GitHub Actions

The project uses GitHub Actions for continuous integration:

- **CI Checks** (`.github/workflows/ci.yml`): Runs on every PR and push
  - Code style validation
  - Build on Ubuntu (GTK3)
  - Build on Fedora (GTK3)
  - Minimal build (no GTK)
  - GTK4 build
  
- **Release Packages** (`.github/workflows/release.yml`): Runs on release
  - Builds RPM and DEB packages
  - Uploads to GitHub Releases

### Local Pre-commit Checks

Before committing, run:

```bash
./scripts/check-style.sh
./scripts/build.sh --clean
```

## Commit Guidelines

### Commit Message Format

```
<subject>

<body>
```

**Subject:**
- Imperative mood ("Add feature", not "Added feature")
- Capitalized first letter
- No period at the end
- Max 50 characters (soft limit)

**Body:**
- Optional, separated by blank line
- Wrap at 72 characters
- Explain *what* and *why*, not *how*

### Examples

```
Add GTK4 support to editor plugin

This enables building the connection editor with GTK4 in addition
to GTK3. The build system now auto-detects available GTK versions.
```

```
Fix memory leak in awg_device_parse_config

The AWGDevice object was not properly unreferenced on error path.
```

### Signing Off

We use DCO (Developer Certificate of Origin). Sign off your commits:

```bash
git commit -s -m "Your commit message"
```

This adds a `Signed-off-by` line with your name and email.

## Pull Request Guidelines

### Before Submitting

- [ ] Code passes `./scripts/check-style.sh`
- [ ] All tests pass (`ctest --output-on-failure`)
- [ ] Commits are squashed if appropriate
- [ ] Commit messages follow guidelines

### PR Description

Include:
1. **What** this PR changes
2. **Why** this change is needed
3. **How** to test (if not obvious)
4. Related issues (e.g., "Fixes #123")

### Review Process

1. CI checks must pass
2. At least one maintainer review required
3. Address review feedback (push new commits, don't force-push)
4. Maintainer will merge when ready

## Architecture Overview

### Directory Structure

```
├── src/              # Service binary (nm-amneziawg-service)
├── properties/       # Connection editor plugin (GTK3/GTK4)
├── shared/           # Shared utility code
│   ├── awg/          # AmneziaWG core module
│   ├── nm-utils/     # NetworkManager utilities
│   └── amneziawg.c   # Netlink helper
├── tests/            # Unit tests
└── po/               # Translations
```

### Key Components

- **AWGDevice** (`shared/awg/awg-device.h`): GObject representation of WireGuard config
- **AWGConnectionManager** (`shared/awg/awg-connection-manager.h`): Abstract interface for tunnel management
- **Service** (`src/nm-amneziawg-service.c`): NM VPN plugin implementation
- **Editor** (`properties/nm-amneziawg-editor.c`): GTK connection editor

### Data Flow

```
WireGuard .conf file
    ↓
awg-config.c (parser)
    ↓
AWGDevice (GObject)
    ↓
awg-nm-connection.c (translator)
    ↓
NMConnection (NetworkManager)
    ↓
AWGConnectionManager (netlink | awg-quick)
    ↓
Kernel module / awg-quick
```

## Localization

### Adding Translations

1. Update source strings with `_()` macro:
   ```c
   g_string_append(error, _("Invalid key format\n"));
   ```

2. Update UI strings in `.ui` files:
   ```xml
   <property name="label" translatable="yes">Private Key</property>
   ```

3. Add files to `po/POTFILES.in`

4. Generate template:
   ```bash
   cd build
   cmake --build . --target pot
   ```

5. Create/update translation:
   ```bash
   msgmerge -U po/ru.po po/NetworkManager-amneziawg.pot
   ```

6. Edit `.po` file and rebuild

## Debugging

### Enable Debug Logging

```bash
# Run NetworkManager with debug
sudo systemctl stop NetworkManager
sudo /usr/sbin/NetworkManager --log-level=DEBUG --log-domains=ALL
```

### View Logs

```bash
# Journalctl
journalctl -u NetworkManager -f

# Filter for amneziawg
journalctl -u NetworkManager | grep -i amneziawg
```

### Common Issues

**"no VPN gateway address received"**
- Ensure peer endpoint is set in configuration
- Check that `NM_VPN_PLUGIN_CONFIG_EXT_GATEWAY` is set in `set_config()`

**Crashes on route processing**
- Check NMIPRoute memory management
- Ensure routes array is properly initialized

**Style check failures**
- Run `./scripts/check-style.sh --fix`
- Check `.clang-format` version compatibility

## Getting Help

- [AGENTS.md](AGENTS.md) - Detailed development guide
- [NetworkManager VPN Plugin API](https://developer.gnome.org/NetworkManager/stable/gdbus-org.freedesktop.NetworkManager.VPN.Plugin.html)
- [GLib Documentation](https://developer.gnome.org/glib/stable/)

## License

By contributing, you agree that your contributions will be licensed under the project's license (GPL-2.0-or-later).
