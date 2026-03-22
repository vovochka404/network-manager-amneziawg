# NetworkManager VPN Plugin for AmneziaWG

[![CI Checks](https://github.com/amnezia-vpn/network-manager-amneziawg/actions/workflows/ci.yml/badge.svg)](https://github.com/amnezia-vpn/network-manager-amneziawg/actions/workflows/ci.yml)
[![Release Packages](https://github.com/amnezia-vpn/network-manager-amneziawg/actions/workflows/release.yml/badge.svg)](https://github.com/amnezia-vpn/network-manager-amneziawg/actions/workflows/release.yml)

A VPN Plugin for NetworkManager that handles client-side AmneziaWG connections. The plugin supports both IPv4 and IPv6 connections and provides a GUI editor for configuring VPN connections.

## Kernel Module Compatibility

**Important:** This plugin version requires **amneziawg kernel module v1.0.20251004 or newer**.

The plugin uses direct netlink communication with the kernel module for optimal performance. The awg-quick fallback mode is still available for systems with old module version.

Requires [amneziawg-tools](https://github.com/amnezia-vpn/amneziawg-tools) to be installed on your system.

---

## Table of Contents

- [Dependencies](#dependencies)
- [Build](#build)
- [Installation](#installation)
- [Usage](#usage)
  - [nmcli Examples](#nmcli-examples)
  - [GUI Usage](#gui-usage)
- [Environment Variables](#environment-variables)
- [Configuration File Example](#configuration-file-example)
- [D-Bus Configuration](#d-bus-configuration)

---

## Dependencies

### Required Dependencies

- **GLib 2** - `glib-2.0`
- **NetworkManager** - `libnm`

### Optional Dependencies

- **amneziawg-tools** - Required for awg-quick mode (`awg-quick` command)

For the GTK editor plugin (one of the following):

- **GTK3**: `gtk+-3.0`, `libnma` (NetworkManager library for GTK3)
- **GTK4**: `gtk4`, `libnma-gtk4` (NetworkManager library for GTK4)

### Build Dependencies

- CMake 3.10+
- pkg-config
- Gettext, intltool (optional, for localization)

---

## Build

The project uses CMake as its build system.

### Basic Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### CMake Options

| Option | Description | Default |
|--------|-------------|---------|
| `WITH_GTK3` | Build with GTK3 editor (requires libnma) | Auto-detect |
| `WITH_GTK4` | Build with GTK4 editor (requires libnma-gtk4) | Auto-detect |
| `CMAKE_INSTALL_PREFIX` | Install prefix | `/usr` |
| `CMAKE_INSTALL_LIBDIR` | Library directory (e.g., lib64) | Auto-detect |

### Build Examples

```bash
# Auto-detect GTK3/GTK4 (default)
cmake ..

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

# Note: For packaging, CMAKE_INSTALL_PREFIX is typically /usr
# The generated .rpm or .deb package can be installed with:
#   sudo rpm -i NetworkManager-amneziawg-*.rpm
#   sudo dpkg -i network-manager-amneziawg_*.deb
```

---

## Installation

```bash
sudo cmake --install .
```

This installs:
- Service plugin: `${libexecdir}/nm-amneziawg-service`
- Editor plugin (if built): `${libdir}/NetworkManager/libnm-vpn-plugin-amneziawg-editor.so`
- D-Bus policy: `${sysconfdir}/dbus-1/system.d/nm-amneziawg-service.conf`
- VPN name file: `${sysconfdir}/NetworkManager/VPN/nm-amneziawg-service.name`

---

## Usage

### nmcli Examples

#### Import existing configuration

```bash
nmcli c import type amneziawg file /path/to/vpn.conf
```

#### Create connection manually

```bash
# Create the VPN connection
nmcli c add type vpn ifname '*' vpn-type amneziawg con-name "My AmneziaWG VPN"

# Set interface private key
nmcli c modify "My AmneziaWG VPN" vpn.data \
  "local-private-key=YAnL1JqN5iMHW2kHbNfT9xLqX5vBz1mQWc8p3Kf9R0E="

# Set peer public key
nmcli c modify "My AmneziaWG VPN" vpn.data \
  "peer-1-public-key=XbK2mPw8nR4tY6vLqZ9hF1cJ3sA5gD7eB9uG2pK0M="

# Set peer endpoint
nmcli c modify "My AmneziaWG VPN" vpn.data \
  "peer-1-endpoint=vpn.example.com:51820"

# Set peer allowed IPs
nmcli c modify "My AmneziaWG VPN" vpn.data \
  "peer-1-allowed-ips=0.0.0.0/0,::/0"

# Activate the connection
nmcli c up "My AmneziaWG VPN"
```

#### List connections

```bash
nmcli c show | grep amneziawg
```

#### View connection details

```bash
nmcli c show "My AmneziaWG VPN"
```

#### Delete connection

```bash
nmcli c delete "My AmneziaWG VPN"
```

### GUI Usage

1. Open **NetworkManager** connection editor:
   - From system tray: Click network icon → Network Settings → Add
   - Or run: `nm-connection-editor`

2. Click **Add** → Select **AmneziaWG** as VPN type

3. Configure the interface:
   - **Private Key**: Enter your base64-encoded private key (required)
   - **Listen Port**: Optional, defaults to auto
   - **MTU**: Optional, 0 for auto-detect
   - **FwMark**: Optional, for firewall marking

4. Add peers in the **Peers** section:
   - **Public Key**: Peer's base64-encoded public key (required)
   - **Endpoint**: Server address (host:port), required
   - **Allowed IPs**: Comma-separated list of subnets (e.g., `0.0.0.0/0,::/0`)
   - **Preshared Key**: Optional, for extra security
   - **Keep-alive**: Optional, seconds

5. Click **Save**

6. Activate: `nmcli c up "Connection Name"`

---

## Environment Variables

The plugin supports the following environment variables to configure its behavior:

| Variable | Description |
|----------|-------------|
| `NM_FORCE_AWG_QUICK` | Force using external awg-quick mode instead of direct netlink |
| `NM_AWG_QUICK_PATH` | Custom path to awg-quick binary (default: looks in PATH) |

### Setting Environment Variables for NetworkManager

To configure these variables system-wide, use `systemctl edit` to create an override:

```bash
# Edit NetworkManager service environment
sudo systemctl edit NetworkManager
```

Add the following content:

```ini
[Service]
Environment="NM_FORCE_AWG_QUICK=1"
```

Or to specify a custom awg-quick path:

```ini
[Service]
Environment="NM_FORCE_AWG_QUICK=1"
Environment="NM_AWG_QUICK_PATH=/usr/bin/awg-quick"
```

After editing, restart NetworkManager:

```bash
sudo systemctl restart NetworkManager
```

### When to Use awg-quick Mode

Use awg-quick mode (set `NM_FORCE_AWG_QUICK=1`) when:

1. Your system have the amneziawg kernel module version lower then v1.0.20251004
2. You prefer the `awg-quick` workflow for tunnel management
3. Debugging connection issues (awg-quick provides more verbose output)

---

## Configuration File Example

A standard WireGuard/AmneziaWG configuration file looks like this:

```ini
[Interface]
# Your private key (required) - generate with: wg genkey
PrivateKey = YAnL1JqN5iMHW2kHbNfT9xLqX5vBz1mQWc8p3Kf9R0E=

# Listen port (optional, auto-assigned if omitted)
ListenPort = 51820

# IP addresses (optional, can be set via NetworkManager IP tabs)
Address = 10.0.0.2/24

# DNS servers (optional, can be set via NetworkManager DNS tab)
DNS = 1.1.1.1, 8.8.8.8

# MTU (optional)
MTU = 1420

# AmneziaWG protocol parameters (optional)
# jc = 16
# jmin = 134
# jmax = 526
# s1 = 34
# s2 = 37
# h1 = 644709644
# h2 = 1603941500
# h3 = 1896912457
# h4 = 2099751415

[Peer]
# Server public key (required)
PublicKey = XbK2mPw8nR4tY6vLqZ9hF1cJ3sA5gD7eB9uG2pK0M=

# Server endpoint (required)
Endpoint = vpn.example.com:51820

# Allowed IPs (required - defines what traffic goes through VPN)
AllowedIPs = 0.0.0.0/0, ::/0

# Persistent keepalive (optional, in seconds)
PersistentKeepalive = 25

# Preshared key (optional, for post-quantum security)
#PresharedKey = somebase64key=
```

To import this configuration into NetworkManager:

```bash
nmcli c import type amneziawg file /path/to/vpn.conf
```

---

## D-Bus Configuration

D-Bus policy is automatically installed to `/etc/dbus-1/system.d/nm-amneziawg-service.conf`.

If you need to manually configure it, the file should contain:

```xml
<!DOCTYPE busconfig PUBLIC
 "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">

<busconfig>
    <policy user="root">
        <allow own_prefix="org.freedesktop.NetworkManager.amneziawg"/>
        <allow send_destination="org.freedesktop.NetworkManager.amneziawg"/>
    </policy>
    <policy context="default">
        <deny own_prefix="org.freedesktop.NetworkDesktop.amneziawg"/>
        <deny send_destination="org.freedesktop.NetworkManager.amneziawg"/>
    </policy>
</busconfig>
```

---

## Viewing Logs

Check NetworkManager logs for debugging:

```bash
# View NetworkManager logs
journalctl -u NetworkManager -f

# Filter for amneziawg specifically
journalctl -u NetworkManager | grep -i amneziawg
```

---

## Resources

- [NetworkManager VPN Plugin Interface](https://developer.gnome.org/NetworkManager/stable/gdbus-org.freedesktop.NetworkManager.VPN.Plugin.html)
- [NMSettingVpn Documentation](https://developer.gnome.org/libnm/stable/NMSettingVpn.html)
- [AmneziaWG Tools](https://github.com/amnezia-vpn/amneziawg-tools)
