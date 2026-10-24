# Optional helper: build an RPM on Fedora, including Fedora Atomic
# (Silverblue, Kinoite, ...) inside a toolbox. See README.md next to this file.
#
# From the top of the source tree:
#   make -C packaging/fedora deps    # build dependencies, once
#   make -C packaging/fedora rpm     # configure, build, test and package
#   make -C packaging/fedora clean
#
# The package ends up in build/rpm/ of the source tree. Goals given together
# run one after another in the order given, also under -j or MAKEFLAGS=-j:
#   make -C packaging/fedora deps rpm

# Paths come from where this file is, not from the caller or the environment.
override SOURCE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
override BUILD_DIR := $(SOURCE_DIR)/build/rpm
# A new release for every build, so that rpm-ostree takes a rebuild of the
# same version as an update.
override RELEASE := $(shell LC_ALL=C date +%Y%m%d%H%M%S)

SUDO := $(if $(filter 0,$(shell id -u)),,sudo)

DEPS := gcc make cmake git pkgconf-pkg-config gettext intltool \
	glib2-devel NetworkManager-libnm-devel \
	gtk3-devel libnma-devel gtk4-devel libnma-gtk4-devel \
	rpm-build

# Both editors are requested explicitly, so that a missing dependency stops
# the build instead of quietly producing a package without an editor.
CMAKE_FLAGS := -DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib64 \
	-DWITH_GTK3=ON \
	-DWITH_GTK4=ON

.PHONY: rpm deps configure build test clean

# Every target here is a step of one sequence, and "deps rpm" or "clean rpm"
# must not configure while dnf is still installing or the tree is being
# removed. Nothing is lost: the compiling is parallel inside cmake --build.
.NOTPARALLEL:

# Compile jobs: those of the -j given to this make, shared through its job
# server (hence the "+" on the recipe), one per CPU without -j. A bare
# "cmake --build --parallel" hands make an unlimited -j instead, which also
# drops the job server and with it the limit given here.
BUILD_JOBS = $(if $(filter -j%,$(MAKEFLAGS)),,--parallel $$(nproc))

rpm: test
	rm -f $(BUILD_DIR)/*.rpm
	cd $(BUILD_DIR) && cpack -G RPM \
		-D CPACK_RPM_PACKAGE_RELEASE=$(RELEASE) \
		-D CPACK_RPM_FILE_NAME=RPM-DEFAULT
	@echo
	@echo "Package:"
	@ls -1 $(BUILD_DIR)/*.rpm

deps:
	$(SUDO) dnf install -y $(DEPS)

configure:
	cmake -S $(SOURCE_DIR) -B $(BUILD_DIR) $(CMAKE_FLAGS)

build: configure
	+cmake --build $(BUILD_DIR) $(BUILD_JOBS)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR)
