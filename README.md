# NetworkManager VPN Plugin for AmneziaWG

[![CI Checks](https://github.com/amnezia-vpn/network-manager-amneziawg/actions/workflows/ci.yml/badge.svg)](https://github.com/amnezia-vpn/network-manager-amneziawg/actions/workflows/ci.yml)
[![Release Packages](https://github.com/amnezia-vpn/network-manager-amneziawg/actions/workflows/release.yml/badge.svg)](https://github.com/amnezia-vpn/network-manager-amneziawg/actions/workflows/release.yml)

A VPN Plugin for NetworkManager that handles client-side AmneziaWG connections. The plugin supports both IPv4 and IPv6 connections and provides a GUI editor for configuring VPN connections.

## Kernel Module Compatibility

**Important:** This plugin version requires **amneziawg kernel module v1.0.20251004 or newer**.

AWG 3.1 profiles additionally require a newer kernel module, because the matching netlink attributes do not exist in older ones: `HeaderProtectionKey`, `ContentPaddingAddition`, `RekeyAfterTime`, `RekeyTimeout`, `RejectAfterTime`, `KeepaliveTimeout` and `MaxHandshakeAttempts` need the **3.0** series or newer, `RandomTrailers` and `DisableCookies` **3.1** or newer. Such a connection is refused with a message naming the parameter instead of failing with `EINVAL`. On the awg-quick fallback the same parameters also need **amneziawg-tools** 3.0 or 3.1 respectively, since older parsers reject the unknown keys. awg-quick does not get around an old module: it creates the interface through the `amneziawg` module whenever one can be loaded (the userspace `amneziawg-go` only when none can), so the module is checked there too, after loading an installed one the way awg-quick would. After upgrading the module, load the new one: reboot, or `sudo modprobe -r amneziawg` while no AmneziaWG interface exists.

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
  - [Secrets](#secrets)
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

- CMake 3.13+ and a C compiler
- pkg-config
- Gettext, intltool (optional, for localization)
- `gtk4-builder-tool` for the GTK4 editor (Fedora: `gtk4-devel`, Debian/Ubuntu: `libgtk-4-bin`, Arch: `gtk4`)
- `dpkg-dev` to build a DEB package (`cpack -G DEB` computes the library dependencies with `dpkg-shlibdeps`)

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
| `WITH_GTK3` | GTK3 editor (requires libnma): `AUTO`, `ON` or `OFF` | `AUTO` |
| `WITH_GTK4` | GTK4 editor (requires libnma-gtk4 and `gtk4-builder-tool`): `AUTO`, `ON` or `OFF` | `AUTO` |
| `CMAKE_INSTALL_PREFIX` | Install prefix | `/usr` |
| `CMAKE_INSTALL_LIBDIR` | Library directory (e.g., lib64) | Auto-detect |
| `NM_PLUGIN_DIR` | Directory of the plugin libraries | `${libdir}/NetworkManager` |
| `NM_VPN_SERVICE_DIR` | Directory of `nm-amneziawg-service.name` | `pkg-config --variable vpnservicedir libnm` |
| `DBUS_SYSTEM_POLICY_DIR` | Directory of the D-Bus policy | `dbus-1/system.d` in `pkg-config --variable datadir dbus-1` |

`AUTO` builds an editor when its dependencies are found; `ON` makes a missing dependency an error. The choice is kept when `cmake` runs again in the same build directory.

Build directories configured by versions in which `WITH_GTK3`/`WITH_GTK4` were plain `ON`/`OFF` options hold `OFF` even where the editors were found, because those versions cached it on the first run. Such an `OFF` is reset to `AUTO` once, with a CMake warning; it cannot be told apart from an `OFF` you chose, so pass `-DWITH_GTK3=OFF` or `-DWITH_GTK4=OFF` again if you meant it. A cached `ON` is kept and becomes a regular `AUTO`/`ON`/`OFF` entry in `ccmake` and `cmake-gui`.

NetworkManager reads `.name` files and the system bus reads policies only from their own directories, so these two files go there whatever `CMAKE_INSTALL_PREFIX` is; with `/usr/local`, for example, NetworkManager still finds the plugin.

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
cmake .. -DWITH_GTK3=ON -DWITH_GTK4=ON -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib64
cmake --build .
cpack -G RPM

# Build DEB package (Debian/Ubuntu)
cmake .. -DWITH_GTK3=ON -DWITH_GTK4=ON -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib
cmake --build .
cpack -G DEB

# Note: For packaging, CMAKE_INSTALL_PREFIX is typically /usr
# The generated .rpm or .deb package can be installed with:
#   sudo rpm -i NetworkManager-amneziawg-*.rpm
#   sudo apt install ./network-manager-amneziawg_*.deb
```

### Fedora RPM helper (optional)

On Fedora, including Fedora Atomic inside a toolbox, `make -C packaging/fedora deps rpm` installs the build dependencies and builds an RPM with both editors. See [packaging/fedora/README.md](packaging/fedora/README.md).

---

## Installation

Every GitHub release carries an RPM built on the current Fedora and a DEB built on Ubuntu 24.04. The DEB depends on the library packages of that release, which have 64-bit `time_t` names such as `libglib2.0-0t64`, so it installs on Ubuntu 24.04 or newer and Debian 13 or newer but not on Ubuntu 22.04 or Debian 12; build the package from source there (see [Build](#build)).

From a build directory:

```bash
sudo cmake --install .
```

This installs:
- Service plugin: `${libexecdir}/nm-amneziawg-service`
- Auth dialog helper (describes the secrets to GNOME Shell, nm-applet and `nmcli`): `${libexecdir}/nm-amneziawg-auth-dialog`
- Plugin libraries: `${libdir}/NetworkManager/libnm-vpn-plugin-amneziawg.so` and `libnm-vpn-plugin-amneziawg-core.so`
- Editor plugins (if built): `${libdir}/NetworkManager/libnm-vpn-plugin-amneziawg-editor.so` (GTK3), `${libdir}/NetworkManager/libnm-gtk4-vpn-plugin-amneziawg-editor.so` (GTK4)
- D-Bus policy: `nm-amneziawg-service.conf` in the system bus policy directory (`DBUS_SYSTEM_POLICY_DIR`, usually `/usr/share/dbus-1/system.d`)
- VPN name file: `nm-amneziawg-service.name` in NetworkManager's VPN directory (`pkg-config --variable vpnservicedir libnm`, usually `/usr/lib/NetworkManager/VPN`)

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

# Set interface private key (a secret, see "Secrets" below)
nmcli c modify "My AmneziaWG VPN" vpn.secrets \
  "local-private-key=YAnL1JqN5iMHW2kHbNfT9xLqX5vBz1mQWc8p3Kf9R0E="

# Set peer public key (peers are numbered from 0)
nmcli c modify "My AmneziaWG VPN" +vpn.data \
  "peer-0-public-key=dj1hrqbuGkEqjI7rDF3tsEvNjyePFZaw2osQxixtjjk="

# Set peer endpoint
nmcli c modify "My AmneziaWG VPN" +vpn.data \
  "peer-0-endpoint=vpn.example.com:51820"

# Set peer allowed IPs (escape the comma: nmcli splits vpn.data at commas)
nmcli c modify "My AmneziaWG VPN" +vpn.data \
  'peer-0-allowed-ips=0.0.0.0/0\,::/0'

# Set the interface address (the "Address" of a .conf). It belongs to the IPv4
# settings (ipv6.addresses for an IPv6 one), not to vpn.data, and a connection
# without any address fails to activate
nmcli c modify "My AmneziaWG VPN" ipv4.addresses 10.8.0.2/32

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

### Secrets

The interface private key (`local-private-key`) and the preshared key of each peer that has one (`peer-N-preshared-key`) are VPN secrets. Where they live is set by their secret flags (`local-private-key-flags`, `peer-N-preshared-key-flags` in `vpn.data`), the same way as for other NetworkManager VPN plugins:

| Flags | Editor menu | Stored |
|-------|-------------|--------|
| `0` | Store the password for all users | by NetworkManager, in the system profile (default for imported keys) |
| `1` | Store the password only for this user | in the user's keyring, by the desktop's secret agent |
| `2` | Ask for this password every time | nowhere, asked on every connection |
| `4` | The password is not required | not used (for a preshared key: the peer has none) |

When a key has to be entered, the desktop's secret agent asks for it: GNOME Shell and nm-applet run `nm-amneziawg-auth-dialog`, which describes the keys, and show their own password dialog; in a terminal, `nmcli --ask connection up <name>` prompts for it. KDE Plasma does not use this helper, so it cannot ask for keys set to "Ask for this password every time".

When GNOME Settings or nm-connection-editor open a saved profile, keys stored for all users come from NetworkManager itself (for users allowed to modify the profile), and keys in the user's keyring from the secret agent. The helper only makes sure that the agent answers this request at once; without it, GNOME Settings waited for a 25-second timeout.

```bash
# Ask for the private key on every connection (NetworkManager then forgets the stored one)
nmcli c modify "My AmneziaWG VPN" +vpn.data local-private-key-flags=2
nmcli --ask c up "My AmneziaWG VPN"
```

#### A stored key went missing

NetworkManager saves a key stored for all users (flags `0`) that comes from a secret agent only if polkit lets that user modify the profile: `org.freedesktop.NetworkManager.settings.modify.own` for a profile restricted to that one user (`connection.permissions`), `org.freedesktop.NetworkManager.settings.modify.system` for any other, such as an imported one. For everyone else, typing the lost key into the desktop's prompt does not bring it back, and an agent may not even be asked: `nmcli c up` then fails with `No valid secrets` or `No agents were available for this request`. The prompt says this and names the command; restore the key as an administrator, with the key in place of `KEY`:

```bash
sudo nmcli c modify "My AmneziaWG VPN" +vpn.secrets local-private-key=KEY
sudo nmcli c modify "My AmneziaWG VPN" +vpn.secrets peer-0-preshared-key=KEY
```

With `nmcli general logging level INFO domains DEFAULT,VPN_PLUGIN:INFO` the service also logs the missing key and these commands.

#### Peers without a preshared key

A peer without a preshared key has neither `peer-N-preshared-key` nor `peer-N-preshared-key-flags`. Version 0.9.12 wrote `peer-N-preshared-key-flags=0` for every peer, both on import and in the editor, so a peer without a `PresharedKey` looks like one whose stored key got lost. The plugin keeps asking for that key rather than connecting without it: `nmcli c up` fails with `No valid secrets`, and an agent that is asked prompts for "Preshared key of peer N (…)", with the same N as in the key name. The prompt itself says what to do: if the peer really has no preshared key, pick "The password is not required" for its preshared key in the editor, or remove the flags as below. An empty answer to the prompt does not mean "no preshared key".

```bash
nmcli -g vpn.data c show "My AmneziaWG VPN" | tr ',' '\n' | grep preshared-key-flags
nmcli c modify "My AmneziaWG VPN" -vpn.data peer-0-preshared-key-flags
```

---

## Environment Variables

The plugin supports the following environment variables to configure its behavior:

| Variable | Description |
|----------|-------------|
| `NM_FORCE_AWG_QUICK` | Force using external awg-quick mode instead of direct netlink |
| `NM_AWG_QUICK_PATH` | Custom path to awg-quick binary (default: the first of `/usr/local/sbin`, `/usr/local/bin`, `/usr/sbin`, `/usr/bin`, `/sbin`, `/bin` that has it; `PATH` is not searched) |

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

1. Your system has an amneziawg kernel module older than v1.0.20251004
2. You prefer the `awg-quick` workflow for tunnel management
3. You are debugging connection issues (awg-quick provides more verbose output)

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
PublicKey = dj1hrqbuGkEqjI7rDF3tsEvNjyePFZaw2osQxixtjjk=

# Server endpoint (required)
Endpoint = vpn.example.com:51820

# Allowed IPs (required - defines what traffic goes through VPN)
AllowedIPs = 0.0.0.0/0, ::/0

# Persistent keepalive (optional, in seconds)
PersistentKeepalive = 25

# Preshared key (optional, for post-quantum security)
#PresharedKey = w+KAKdxflwUGTsMC0Ljj1tXso4T1V+F8k8WcSmpbWAk=
```

To import this configuration into NetworkManager:

```bash
nmcli c import type amneziawg file /path/to/vpn.conf
```

---

## D-Bus Configuration

D-Bus policy is installed below the data directory of D-Bus,
`dbus-1/system.d` in `pkg-config --variable datadir dbus-1` (usually
`/usr/share/dbus-1/system.d`). Do not copy an additional policy with a global
`context="default"` deny rule into `/etc/dbus-1/system.d`.

If you need to manually configure it, the file should contain:

```xml
<!DOCTYPE busconfig PUBLIC
 "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">

<busconfig>
    <policy user="root">
        <allow own="org.freedesktop.NetworkManager.amneziawg"/>
        <allow send_destination="org.freedesktop.NetworkManager.amneziawg"/>
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
