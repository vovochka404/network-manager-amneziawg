# Building an RPM on Fedora (optional helper)

`packaging/fedora/Makefile` is a convenience for building an RPM of the plugin on
Fedora with both editors (GTK3 and GTK4). It is not needed for a regular build
(see [Build](../../README.md#build)) and is not a Fedora package spec.

It wraps the generic steps: `cmake -DWITH_GTK3=ON -DWITH_GTK4=ON`, `ctest` and
`cpack -G RPM`. Every run gets a new RPM release number, so a rebuild of the same
version is still an update for `dnf` and `rpm-ostree`.

The version itself comes from `git describe`: `0.9.12-5-g<commit>` five commits after
the `v0.9.12` tag, which RPM orders correctly. A clone without release tags (for
example `git clone --depth 1`) yields `0.9.5-dev-<commit>` instead. RPM cannot order
two such versions by their commit hashes, and every one of them is older than a
release such as 0.9.12. The commands below install the new package anyway (`dnf
install` downgrades if it has to), `rpm -U` refuses it; run
`git fetch --unshallow --tags` once to get ordered versions.

Run it from the top of the source tree as a regular user; `deps` calls `sudo` when
needed:

```bash
make -C packaging/fedora deps    # build dependencies, once
make -C packaging/fedora rpm     # configure, build, run the tests, package
make -C packaging/fedora clean   # remove build/rpm
```

Goals can be combined, `make -C packaging/fedora deps rpm` or `clean rpm`: they run
one after another in the order given, also with `-j` or `MAKEFLAGS=-j`. The compile
step runs one job per CPU, or as many as a given `-j` allows (`make -j2 -C
packaging/fedora rpm` on a machine short of memory). The package is written to
`build/rpm/`.

## Fedora Workstation

```bash
sudo dnf install ./build/rpm/networkmanager-amneziawg-*.rpm
```

## Fedora Atomic (Silverblue, Kinoite, ...)

Build inside a toolbox whose release matches the host, then install on the host.

```bash
toolbox create --release 45      # once, with the release of the host
toolbox enter
sudo dnf install -y make         # once, the toolbox image has no make
make -C packaging/fedora deps
make -C packaging/fedora rpm
exit
```

On the host (outside the toolbox), layer the package and reboot:

```bash
rpm-ostree install ./build/rpm/networkmanager-amneziawg-*.rpm
systemctl reboot
```

To replace a build that is already layered:

```bash
rpm-ostree uninstall networkmanager-amneziawg --install ./build/rpm/networkmanager-amneziawg-*.rpm
systemctl reboot
```

## Runtime

The package contains the plugin only. Connections also need a backend, installed
separately: the `amneziawg` kernel module, or `amneziawg-tools` (`awg-quick`) together
with the module or the `amneziawg-go` userspace implementation. See
[Kernel Module Compatibility](../../README.md#kernel-module-compatibility).
