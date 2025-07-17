AUTOMAKE_OPTIONS = foreign subdir-objects

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-more-warnings=yes

libexec_PROGRAMS =

noinst_LTLIBRARIES =

EXTRA_DIST =

CLEANFILES =

check_programs =

noinst_PROGRAMS =

SUBDIRS = \
	. \
	po

plugindir = $(libdir)/NetworkManager
plugin_LTLIBRARIES =

dbusservicedir = $(sysconfdir)/dbus-1/system.d
dbusservice_DATA = nm-amneziawg-service.conf

nmvpnservicedir = $(NM_VPN_SERVICE_DIR)
nmvpnservice_DATA = nm-amneziawg-service.name

uidir = $(datadir)/gnome-vpn-properties/amneziawg
ui_DATA =

###############################################################################

shared_sources = \
	shared/nm-utils/nm-shared-utils.c \
	shared/utils.c \
	shared/import-export.c \
	shared/amneziawg.c

###############################################################################

src_cppflags = \
	-DBINDIR=\"$(bindir)\" \
	-DPREFIX=\""$(prefix)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	-DLOCALSTATEDIR=\""$(localstatedir)"\" \
	-DDATADIR=\"$(datadir)\" \
	-DNM_AMNEZIAWG_LOCALEDIR=\"$(datadir)/locale\" \
	-DG_LOG_DOMAIN=\"nm-amneziawg\" \
	-I$(srcdir)/shared \
	$(GLIB_CFLAGS) \
	$(LIBNM_CFLAGS)

noinst_LTLIBRARIES += src/libnm-utils.la

src_libnm_utils_la_SOURCES = $(shared_sources)
src_libnm_utils_la_CPPFLAGS = $(src_cppflags)
src_libnm_utils_la_LIBADD = \
	$(GLIB_LIBS) \
	$(LIBNM_LIBS)

libexec_PROGRAMS += src/nm-amneziawg-service

src_nm_amneziawg_service_CPPFLAGS = $(src_cppflags)
src_nm_amneziawg_service_LDFLAGS = \
	-Wl,--version-script="$(srcdir)/linker-script-binary.ver"
src_nm_amneziawg_service_LDADD = \
	src/libnm-utils.la \
	$(GLIB_LIBS) \
	$(LIBNM_LIBS)
EXTRA_src_nm_amneziawg_service_DEPENDENCIES = \
	linker-script-binary.ver

###############################################################################

properties/resources.h: properties/gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ --sourcedir=$(srcdir)/properties --generate-header --internal

properties/resources.c: properties/gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir)/properties --generate-dependencies $(srcdir)/properties/gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ --sourcedir=$(srcdir)/properties --generate-source --internal

CLEANFILES += \
	properties/resources.c \
	properties/resources.h

EXTRA_DIST += \
	properties/gresource.xml

plugin_sources = \
	properties/nm-amneziawg-editor-plugin.c \
	properties/nm-amneziawg-editor-plugin.h

editor_sources = \
	properties/resources.c \
	properties/resources.h \
	properties/nm-amneziawg-editor.c \
	properties/nm-amneziawg-editor.h

properties_cppflags = \
	-DICONDIR=\""$(datadir)/pixmaps"\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-I$(srcdir)/shared \
	$(GLIB_CFLAGS)

###############################################################################

noinst_LTLIBRARIES += properties/libnm-vpn-plugin-amneziawg-utils.la

properties_libnm_vpn_plugin_amneziawg_utils_la_SOURCES = \
	$(shared_sources)

properties_libnm_vpn_plugin_amneziawg_utils_la_CPPFLAGS = \
	-DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB_BASE \
	-DNM_PLUGIN_DIR=\"$(NM_PLUGIN_DIR)\" \
	$(properties_cppflags) \
	$(LIBNM_CFLAGS)

properties_libnm_vpn_plugin_amneziawg_utils_la_LIBADD = \
	$(LIBNM_LIBS) \
	$(DL_LIBS)

noinst_LTLIBRARIES += properties/libnm-vpn-plugin-amneziawg-core.la

properties_libnm_vpn_plugin_amneziawg_core_la_SOURCES = \
	shared/nm-utils/nm-vpn-plugin-utils.c \
	shared/nm-utils/nm-vpn-plugin-utils.h \
	shared/nm-utils/nm-vpn-plugin-macros.h \
	$(plugin_sources)

properties_libnm_vpn_plugin_amneziawg_core_la_CPPFLAGS = \
	-DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB_BASE \
	-DNM_PLUGIN_DIR=\"$(NM_PLUGIN_DIR)\" \
	$(properties_cppflags) \
	$(LIBNM_CFLAGS)

properties_libnm_vpn_plugin_amneziawg_core_la_LIBADD = \
	properties/libnm-vpn-plugin-amneziawg-utils.la \
	$(LIBNM_LIBS) \
	$(DL_LIBS)

plugin_LTLIBRARIES += properties/libnm-vpn-plugin-amneziawg.la

properties_libnm_vpn_plugin_amneziawg_la_SOURCES =
properties_libnm_vpn_plugin_amneziawg_la_LIBADD = \
	properties/libnm-vpn-plugin-amneziawg-core.la
properties_libnm_vpn_plugin_amneziawg_la_LDFLAGS = \
	-avoid-version \
	-Wl,--version-script=$(srcdir)/properties/libnm-vpn-plugin-amneziawg.ver

###############################################################################

if WITH_GNOME
plugin_LTLIBRARIES += properties/libnm-vpn-plugin-amneziawg-editor.la
endif

properties_libnm_vpn_plugin_amneziawg_editor_la_SOURCES = \
	$(editor_sources)

properties_libnm_vpn_plugin_amneziawg_editor_la_CPPFLAGS = \
	-DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB_EDITOR \
	$(properties_cppflags) \
	$(GTK_CFLAGS) \
	$(LIBNM_CFLAGS) \
	$(LIBNMA_CFLAGS)

properties_libnm_vpn_plugin_amneziawg_editor_la_LIBADD = \
	properties/libnm-vpn-plugin-amneziawg-utils.la \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBNM_LIBS) \
	$(LIBNMA_LIBS)

properties_libnm_vpn_plugin_amneziawg_editor_la_LDFLAGS = \
	-avoid-version \
	-Wl,--version-script=$(srcdir)/properties/libnm-vpn-plugin-amneziawg-editor.ver

###############################################################################

if WITH_GNOME
noinst_LTLIBRARIES += properties/libnm-amneziawg-properties-core.la
endif

properties_libnm_amneziawg_properties_core_la_SOURCES = \
	$(shared_sources) \
	$(plugin_sources) \
	$(editor_sources)

properties_libnm_amneziawg_properties_core_la_CPPFLAGS = \
	-DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB \
	-DNM_PLUGIN_DIR=\"$(NM_PLUGIN_DIR)\" \
	$(properties_cppflags) \
	$(GTK_CFLAGS) \
	$(LIBNM_CFLAGS)

properties_libnm_amneziawg_properties_core_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBNM_LIBS)


if WITH_GNOME
plugin_LTLIBRARIES += properties/libnm-amneziawg-properties.la
endif

properties_libnm_amneziawg_properties_la_SOURCES =
properties_libnm_amneziawg_properties_la_LIBADD = \
	properties/libnm-amneziawg-properties-core.la
properties_libnm_amneziawg_properties_la_LDFLAGS = \
	-avoid-version \
	-Wl,--version-script=$(srcdir)/properties/libnm-amneziawg-properties.ver

EXTRA_DIST += \
	properties/libnm-vpn-plugin-amneziawg.ver \
	properties/libnm-vpn-plugin-amneziawg-editor.ver \
	properties/libnm-amneziawg-properties.ver \
	properties/nm-amneziawg-dialog.ui

###############################################################################

if WITH_GNOME
libexec_PROGRAMS += auth-dialog/nm-amneziawg-auth-dialog
endif

auth_dialog_nm_amneziawg_auth_dialog_SOURCES = \
	$(shared_sources) \
	auth-dialog/main.c
auth_dialog_nm_amneziawg_auth_dialog_CPPFLAGS = \
	-DICONDIR=\""$(datadir)/pixmaps"\" \
	-DBINDIR=\""$(bindir)"\" \
	-DGNOMELOCALEDIR=\"$(datadir)/locale\" \
	-I$(srcdir)/shared \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBNM_CFLAGS) \
	$(LIBNMA_CFLAGS) \
	$(LIBSECRET_CFLAGS)
auth_dialog_nm_amneziawg_auth_dialog_LDFLAGS = \
	-Wl,--version-script="$(srcdir)/linker-script-binary.ver"
auth_dialog_nm_amneziawg_auth_dialog_LDADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(GTK_LIBS) \
	$(LIBNM_LIBS) \
	$(LIBNMA_LIBS) \
	$(LIBSECRET_LIBS)
EXTRA_auth_dialog_nm_amneziawg_auth_dialog_DEPENDENCIES = \
	linker-script-binary.ver

###############################################################################

appdatadir = $(datadir)/appdata
appdata_files = $(appdata_in_files:.xml.in=.xml)
if WITH_GNOME
appdata_DATA = $(appdata_files)
endif
appdata_in_files = appdata/network-manager-amneziawg.metainfo.xml.in
@INTLTOOL_XML_RULE@

nm-amneziawg-service.name: $(srcdir)/nm-amneziawg-service.name.in
	$(AM_V_GEN) sed -e 's|[@]LIBEXECDIR[@]|$(libexecdir)|g' \
	    -e 's|[@]PLUGINDIR[@]/|@NM_PLUGIN_DIR_NAME_FILE@|g' \
	    $^ >$@

# we always build the tests during regular build.
noinst_PROGRAMS += $(check_programs)

TESTS = $(check_programs)

EXTRA_DIST += \
	linker-script-binary.ver \
	nm-amneziawg-service.name.in \
	$(dbusservice_DATA) \
	$(appdata_in_files) \
	$(appdata_files) \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	\
	shared/README \
	shared/nm-utils/gsystem-local-alloc.h \
	shared/nm-utils/nm-glib.h \
	shared/nm-utils/nm-macros-internal.h \
	shared/nm-utils/nm-shared-utils.c \
	shared/nm-utils/nm-shared-utils.h \
	shared/nm-utils/nm-test-utils.h \
	shared/nm-default.h \
	shared/nm-service-defines.h \
	shared/utils.c \
	shared/utils.h \
	shared/import-export.c \
	shared/import-export.h \
	$(NULL)

CLEANFILES += \
	$(nmvpnservice_DATA) \
	$(appdata_files)

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

ACLOCAL_AMFLAGS = -I m4
