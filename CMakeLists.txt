cmake_minimum_required(VERSION 3.10)

# Get version from git tag if available, otherwise use default
find_package(Git QUIET)
set(PROJECT_VERSION_DEFAULT "0.9.5")
set(PROJECT_VERSION_STRING "${PROJECT_VERSION_DEFAULT}")
set(PROJECT_VERSION_SUFFIX "")

if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(GIT_VERSION)
        # Check if version is from tag (contains dots) or commit hash
        # Match patterns like: v1.2.3, 1.2.3, v1.2.3-dirty, 1.2.3-2-g1234567
        if(GIT_VERSION MATCHES "^v?([0-9]+\\.[0-9]+\\.[0-9]+)(-.*)?$")
            # Valid semver from tag, e.g., "v1.2.3" or "v1.2.3-2-g1234567-dirty"
            set(PROJECT_VERSION_STRING "${CMAKE_MATCH_1}")
            if(CMAKE_MATCH_2)
                set(PROJECT_VERSION_SUFFIX "${CMAKE_MATCH_2}")
            endif()
        else()
            # Commit hash without tag, e.g., "28da31d-dirty" or "g1234567"
            # Use default version with commit hash as suffix
            set(PROJECT_VERSION_STRING "${PROJECT_VERSION_DEFAULT}")
            set(PROJECT_VERSION_SUFFIX "-dev-${GIT_VERSION}")
        endif()
    endif()
endif()

project(NetworkManager-amneziawg VERSION ${PROJECT_VERSION_STRING})

# Set full version string with suffix for packaging
set(PROJECT_VERSION_FULL "${PROJECT_VERSION_STRING}${PROJECT_VERSION_SUFFIX}")

enable_testing()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WITH_GNOME "Build with GNOME support" ON)
option(ENABLE_ABSOLUTE_PATHS "Use absolute paths in .name files" OFF)

include(GNUInstallDirs)

# Set default prefix to /usr for system packages (RPM)
if(NOT DEFINED CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix" FORCE)
endif()

# Set default locale dir if not set
if(NOT CMAKE_INSTALL_LOCALEDIR)
    set(CMAKE_INSTALL_LOCALEDIR "share/locale" CACHE PATH "Locale directory" FORCE)
endif()

# Use absolute path for LOCALEDIR
set(LOCALEDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LOCALEDIR}")

set(DIST_VERSION "0.9.0" CACHE STRING "Distribution version string")

set(PACKAGE_NAME "NetworkManager-amneziawg")
set(GETTEXT_PACKAGE "NetworkManager-amneziawg")
set(EXEC_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" CACHE PATH "Prefix for executables")

find_package(Gettext)
if(GETTEXT_FOUND)
    set(ENABLE_NLS ON)
    message(STATUS "Gettext found - NLS support enabled")
else()
    set(ENABLE_NLS OFF)
    message(STATUS "Gettext not found - NLS support disabled")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=gnu99")
set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_RELEASE "-O2")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LIBNM REQUIRED libnm)

if(WITH_GNOME)
    pkg_check_modules(GTK REQUIRED gtk+-3.0)
    pkg_check_modules(LIBNMA REQUIRED libnma)
    pkg_check_modules(LIBSECRET libsecret-1)
    if(NOT LIBSECRET_FOUND)
        message(WARNING "libsecret-1 not found, disabling GNOME support")
        set(WITH_GNOME OFF CACHE BOOL "Build with GNOME support" FORCE)
    endif()
endif()

# Get VPN service dir from pkg-config (respects prefix and libdir)
execute_process(
    COMMAND pkg-config --variable vpnservicedir libnm
    OUTPUT_VARIABLE NM_VPN_SERVICE_DIR_RAW
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NM_VPN_SERVICE_DIR_RAW)
    set(NM_VPN_SERVICE_DIR "${NM_VPN_SERVICE_DIR_RAW}" CACHE PATH "NetworkManager VPN service directory")
else()
    set(NM_VPN_SERVICE_DIR "${CMAKE_INSTALL_LIBDIR}/NetworkManager/VPN" CACHE PATH "NetworkManager VPN service directory")
endif()

set(NM_PLUGIN_DIR "${CMAKE_INSTALL_LIBDIR}/NetworkManager" CACHE PATH "NetworkManager plugin directory")

set(NM_PLUGIN_DIR_NAME_FILE "")
if(ENABLE_ABSOLUTE_PATHS)
    set(NM_PLUGIN_DIR_NAME_FILE "${NM_PLUGIN_DIR}/")
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/nm-amneziawg-service.name.in
    ${CMAKE_SOURCE_DIR}/nm-amneziawg-service.name
)

include(GenerateExportHeader)

configure_file(
    ${CMAKE_SOURCE_DIR}/config.h.cmake
    ${CMAKE_BINARY_DIR}/config.h
)

set(CMAKE_INCLUDE_CURRENT_DIR INCLUDE)

set(SHARED_SOURCES
    shared/nm-utils/nm-shared-utils.c
    shared/utils.c
    shared/amneziawg.c
)

set(COMMON_CFLAGS
    ${GLIB_CFLAGS}
    ${LIBNM_CFLAGS}
    -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_32
    -DNM_VERSION_MIN_REQUIRED=NM_VERSION_1_14
    -DNM_VERSION_MAX_ALLOWED=NM_VERSION_1_52
    -DG_LOG_DOMAIN=\"nm-amneziawg\"
    -I${CMAKE_SOURCE_DIR}/shared
)

set(COMMON_DEFINITIONS
    -DBINDIR="${CMAKE_INSTALL_BINDIR}"
    -DPREFIX="${CMAKE_INSTALL_PREFIX}"
    -DSYSCONFDIR="${CMAKE_INSTALL_SYSCONFDIR}"
    -DLIBDIR="${CMAKE_INSTALL_LIBDIR}"
    -DLIBEXECDIR="${CMAKE_INSTALL_LIBEXECDIR}"
    -DLOCALSTATEDIR="${CMAKE_INSTALL_LOCALSTATEDIR}"
    -DDATADIR="${CMAKE_INSTALL_DATADIR}"
    -DNM_AMNEZIAWG_LOCALEDIR="${CMAKE_INSTALL_LOCALEDIR}"
)

if(WITH_GNOME)
    list(APPEND COMMON_CFLAGS ${GTK_CFLAGS} -DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_3_16)
endif()

set(PROPERTIES_RESOURCES_DIR ${CMAKE_SOURCE_DIR}/properties)
set(PROPERTIES_RESOURCES_XML ${CMAKE_SOURCE_DIR}/properties/gresource.xml)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/properties)

add_custom_command(
    OUTPUT properties/resources.h
    COMMAND /usr/bin/glib-compile-resources
        ${PROPERTIES_RESOURCES_XML}
        --target=properties/resources.h
        --sourcedir=${PROPERTIES_RESOURCES_DIR}
        --generate-header
        --internal
    DEPENDS ${PROPERTIES_RESOURCES_XML}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_command(
    OUTPUT properties/resources.c
    COMMAND /usr/bin/glib-compile-resources
        ${PROPERTIES_RESOURCES_XML}
        --target=properties/resources.c
        --sourcedir=${PROPERTIES_RESOURCES_DIR}
        --generate-source
        --internal
    DEPENDS ${PROPERTIES_RESOURCES_XML}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(gresources ALL DEPENDS properties/resources.h properties/resources.c)

set(PROPERTIES_CPPFLAGS
    -DICONDIR=\"${CMAKE_INSTALL_PREFIX}/share/pixmaps\"
    -DLOCALEDIR=\"${LOCALEDIR}\"
    -I${CMAKE_SOURCE_DIR}/shared
)

add_executable(nm-amneziawg-service src/nm-amneziawg-service.c
    ${SHARED_SOURCES}
    shared/awg/awg-config.c
    shared/awg/awg-device.c
    shared/awg/awg-nm-connection.c
    shared/awg/awg-connection-manager.c
    shared/awg/awg-connection-manager-dummy.c
    shared/awg/awg-connection-manager-external.c
    shared/awg/awg-connection-manager-netlink.c
)
target_include_directories(nm-amneziawg-service PRIVATE ${CMAKE_SOURCE_DIR}/shared ${CMAKE_BINARY_DIR})
target_link_libraries(nm-amneziawg-service PRIVATE ${GLIB_LIBRARIES} ${LIBNM_LIBRARIES})
target_compile_options(nm-amneziawg-service PRIVATE ${COMMON_CFLAGS} ${COMMON_DEFINITIONS})
set_target_properties(nm-amneziawg-service PROPERTIES OUTPUT_NAME "nm-amneziawg-service" LINKER_LANGUAGE C)

add_executable(nm-amneziawg-test tests/test-main.c
    shared/awg/awg-config.c
    shared/awg/awg-device.c
    shared/awg/awg-nm-connection.c
    shared/awg/awg-connection-manager.c
    shared/awg/awg-connection-manager-dummy.c
    shared/awg/awg-connection-manager-external.c
    shared/awg/awg-connection-manager-netlink.c
    shared/amneziawg.c
)
target_include_directories(nm-amneziawg-test PRIVATE ${CMAKE_SOURCE_DIR}/shared ${CMAKE_BINARY_DIR})
target_link_libraries(nm-amneziawg-test PRIVATE ${GLIB_LIBRARIES} ${LIBNM_LIBRARIES})
target_compile_options(nm-amneziawg-test PRIVATE ${COMMON_CFLAGS} ${COMMON_DEFINITIONS})

file(GLOB TEST_CONFIGS "${CMAKE_SOURCE_DIR}/tests/*.conf")
foreach(config ${TEST_CONFIGS})
    get_filename_component(config_name ${config} NAME)
    configure_file(${config} ${CMAKE_BINARY_DIR}/tests/${config_name} COPYONLY)
endforeach()

add_test(NAME amneziawg-test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} COMMAND nm-amneziawg-test)

set(VPN_PLUGIN_SOURCES
    properties/nm-amneziawg-editor-plugin.c
    properties/nm-amneziawg-editor-plugin.h
    shared/awg/awg-device.c
    shared/awg/awg-config.c
    shared/awg/awg-nm-connection.c
)

set(EDITOR_SOURCES
    properties/nm-amneziawg-editor.c
    properties/nm-amneziawg-editor.h
)

add_library(nm-vpn-plugin-amneziawg-utils OBJECT ${SHARED_SOURCES})
target_include_directories(nm-vpn-plugin-amneziawg-utils PUBLIC ${CMAKE_SOURCE_DIR}/shared)
target_include_directories(nm-vpn-plugin-amneziawg-utils PRIVATE ${CMAKE_BINARY_DIR})
target_compile_options(nm-vpn-plugin-amneziawg-utils PRIVATE
    ${COMMON_CFLAGS} ${COMMON_DEFINITIONS}
    -DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB_BASE
    -DNM_PLUGIN_DIR=\"${NM_PLUGIN_DIR}\"
)
target_link_libraries(nm-vpn-plugin-amneziawg-utils PRIVATE ${LIBNM_LIBRARIES} ${CMAKE_DL_LIBS})
set_target_properties(nm-vpn-plugin-amneziawg-utils PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(nm-vpn-plugin-amneziawg-core OBJECT
    shared/nm-utils/nm-vpn-plugin-utils.c
    shared/nm-utils/nm-vpn-plugin-utils.h
    shared/nm-utils/nm-vpn-plugin-macros.h
    ${VPN_PLUGIN_SOURCES}
)
target_include_directories(nm-vpn-plugin-amneziawg-core PUBLIC ${CMAKE_SOURCE_DIR}/shared)
target_include_directories(nm-vpn-plugin-amneziawg-core PRIVATE ${CMAKE_BINARY_DIR})
target_sources(nm-vpn-plugin-amneziawg-core PRIVATE properties/resources.h properties/resources.c)
target_compile_options(nm-vpn-plugin-amneziawg-core PRIVATE
    ${COMMON_CFLAGS} ${COMMON_DEFINITIONS} ${PROPERTIES_CPPFLAGS}
    -DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB_BASE
    -DNM_PLUGIN_DIR=\"${NM_PLUGIN_DIR}\"
)
target_link_libraries(nm-vpn-plugin-amneziawg-core PRIVATE
    nm-vpn-plugin-amneziawg-utils ${LIBNM_LIBRARIES} ${CMAKE_DL_LIBS}
)
set_target_properties(nm-vpn-plugin-amneziawg-core PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(nm-vpn-plugin-amneziawg SHARED)
target_sources(nm-vpn-plugin-amneziawg PRIVATE
    $<TARGET_OBJECTS:nm-vpn-plugin-amneziawg-core>
    $<TARGET_OBJECTS:nm-vpn-plugin-amneziawg-utils>
)
target_link_libraries(nm-vpn-plugin-amneziawg PRIVATE ${LIBNM_LIBRARIES} ${CMAKE_DL_LIBS})
set_target_properties(nm-vpn-plugin-amneziawg PROPERTIES
    OUTPUT_NAME "nm-vpn-plugin-amneziawg"
)

if(WITH_GNOME)
    add_library(nm-vpn-plugin-amneziawg-editor SHARED ${EDITOR_SOURCES})
    target_include_directories(nm-vpn-plugin-amneziawg-editor PRIVATE ${CMAKE_SOURCE_DIR}/shared ${CMAKE_BINARY_DIR})
    target_link_libraries(nm-vpn-plugin-amneziawg-editor PRIVATE
        nm-vpn-plugin-amneziawg-utils ${GLIB_LIBRARIES} ${GTK_LIBRARIES} ${LIBNM_LIBRARIES} ${LIBNMA_LIBRARIES}
    )
    target_compile_options(nm-vpn-plugin-amneziawg-editor PRIVATE
        ${COMMON_CFLAGS} ${COMMON_DEFINITIONS} ${PROPERTIES_CPPFLAGS}
        -DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB_EDITOR
    )
    set_target_properties(nm-vpn-plugin-amneziawg-editor PROPERTIES
        OUTPUT_NAME "nm-vpn-plugin-amneziawg-editor"
    )

    add_library(nm-amneziawg-properties-core OBJECT
        ${VPN_PLUGIN_SOURCES} ${EDITOR_SOURCES}
    )
    target_include_directories(nm-amneziawg-properties-core PUBLIC ${CMAKE_SOURCE_DIR}/shared)
    target_include_directories(nm-amneziawg-properties-core PRIVATE ${CMAKE_BINARY_DIR})
    target_sources(nm-amneziawg-properties-core PRIVATE properties/resources.h properties/resources.c)
    target_compile_options(nm-amneziawg-properties-core PRIVATE
        ${COMMON_CFLAGS} ${COMMON_DEFINITIONS} ${PROPERTIES_CPPFLAGS}
        -DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB
        -DNM_PLUGIN_DIR=\"${NM_PLUGIN_DIR}\"
    )
    target_link_libraries(nm-amneziawg-properties-core PRIVATE ${GLIB_LIBRARIES} ${GTK_LIBRARIES} ${LIBNM_LIBRARIES})
    set_target_properties(nm-amneziawg-properties-core PROPERTIES POSITION_INDEPENDENT_CODE ON)

    add_library(nm-amneziawg-properties SHARED)
    target_sources(nm-amneziawg-properties PRIVATE
        $<TARGET_OBJECTS:nm-amneziawg-properties-core>
        $<TARGET_OBJECTS:nm-vpn-plugin-amneziawg-utils>
    )
    target_link_libraries(nm-amneziawg-properties PRIVATE ${GLIB_LIBRARIES} ${GTK_LIBRARIES} ${LIBNM_LIBRARIES})
    set_target_properties(nm-amneziawg-properties PROPERTIES
        OUTPUT_NAME "nm-amneziawg-properties"
    )
endif()

install(TARGETS nm-amneziawg-service RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(TARGETS nm-vpn-plugin-amneziawg LIBRARY DESTINATION ${NM_PLUGIN_DIR})

if(WITH_GNOME)
    install(TARGETS nm-vpn-plugin-amneziawg-editor LIBRARY DESTINATION ${NM_PLUGIN_DIR})
    install(TARGETS nm-amneziawg-properties LIBRARY DESTINATION ${NM_PLUGIN_DIR}/properties)
    install(FILES properties/nm-amneziawg-dialog.ui DESTINATION ${CMAKE_INSTALL_DATADIR}/gnome-vpn-properties/amneziawg)
endif()

install(FILES nm-amneziawg-service.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/system.d)
install(FILES ${CMAKE_BINARY_DIR}/nm-amneziawg-service.name DESTINATION ${NM_VPN_SERVICE_DIR})

if(ENABLE_NLS)
    set(AM_CPPFLAGS ${AM_CPPFLAGS} -DENABLE_NLS)
    set(AM_CPPFLAGS ${AM_CPPFLAGS} -DGETTEXT_PACKAGE=\"${GETTEXT_PACKAGE}\")
    include_directories(${GETTEXT_INCLUDE_DIR})

    find_program(INTLTOOL_MERGE intltool-merge)
    if(NOT INTLTOOL_MERGE)
        message(WARNING "intltool-merge not found, appdata localization disabled")
    else()
        message(STATUS "Found intltool-merge: ${INTLTOOL_MERGE}")
    endif()

    set(PO_DIRECTORY ${CMAKE_SOURCE_DIR}/po)
    file(GLOB PO_FILES "${PO_DIRECTORY}/*.po")
    list(SORT PO_FILES)
    set(GMO_FILES)
    foreach(PO_FILE ${PO_FILES})
        get_filename_component(LANG ${PO_FILE} NAME_WE)
        list(APPEND GMO_FILES ${CMAKE_BINARY_DIR}/${LANG}.gmo)
    endforeach()

    set(POT_FILE ${CMAKE_BINARY_DIR}/${GETTEXT_PACKAGE}.pot)
    set(POTFILES_IN ${PO_DIRECTORY}/POTFILES.in)

    find_program(GETTEXT_XGETTEXT xgettext)
    if(GETTEXT_XGETTEXT)
        add_custom_command(
            OUTPUT ${POT_FILE}
            COMMAND grep -v '^\\[' ${POTFILES_IN} \| xgettext --directory=${CMAKE_SOURCE_DIR} --files-from=/dev/stdin -o ${POT_FILE}
            DEPENDS ${POTFILES_IN}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        add_custom_target(pot ALL DEPENDS ${POT_FILE})
    endif()

    foreach(PO_IN ${PO_FILES})
        get_filename_component(LANG ${PO_IN} NAME_WE)
        set(GMO_OUT ${CMAKE_BINARY_DIR}/${LANG}.gmo)
        add_custom_command(
            OUTPUT ${GMO_OUT}
            COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${GMO_OUT} ${PO_IN}
            DEPENDS ${PO_IN}
        )
        list(APPEND GMO_FILES_BUILD ${GMO_OUT})
    endforeach()

    add_custom_target(translations ALL
        DEPENDS ${GMO_FILES_BUILD}
    )

    set(APP_DATA_IN ${CMAKE_SOURCE_DIR}/appdata/network-manager-amneziawg.metainfo.xml.in)
    set(APP_DATA_OUT ${CMAKE_BINARY_DIR}/appdata/network-manager-amneziawg.metainfo.xml)

    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/appdata)

    add_custom_command(
        OUTPUT ${APP_DATA_OUT}
        COMMAND ${INTLTOOL_MERGE} -d -u ${PO_DIRECTORY} ${APP_DATA_IN} ${APP_DATA_OUT}
        DEPENDS ${APP_DATA_IN} ${GMO_FILES_BUILD}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    add_custom_target(appdata ALL
        DEPENDS ${APP_DATA_OUT}
    )

    foreach(GMO_FILE ${GMO_FILES_BUILD})
        get_filename_component(LANG ${GMO_FILE} NAME_WE)
        install(FILES ${GMO_FILE} DESTINATION ${CMAKE_INSTALL_LOCALEDIR}/${LANG}/LC_MESSAGES RENAME ${GETTEXT_PACKAGE}.mo)
    endforeach()

    install(FILES ${APP_DATA_OUT} DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
endif()

message("")
message("Build configuration:")
message("  WITH_GNOME: ${WITH_GNOME}")
message("  ENABLE_ABSOLUTE_PATHS: ${ENABLE_ABSOLUTE_PATHS}")
message("  ENABLE_NLS: ${ENABLE_NLS}")
message("  PROJECT_VERSION: ${PROJECT_VERSION}")
message("  PROJECT_VERSION_FULL: ${PROJECT_VERSION_FULL}")
message("")

# CPack common configuration
set(CPACK_PACKAGE_NAME "NetworkManager-amneziawg")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION_FULL}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "NetworkManager VPN plugin for AmneziaWG")
set(CPACK_PACKAGE_VENDOR "vovochka404")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Package Maintainer <maintainer@example.com>")
set(CPACK_PACKAGE_URL "https://github.com/amnezia-vpn/network-manager-amneziawg")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

# CPack configuration for RPM
set(CPACK_RPM_INSTALL_PREFIX "/usr")
set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "System/Network")
set(CPACK_RPM_PACKAGE_REQUIRES "NetworkManager >= 1.0, glib2 >= 2.0")

# CPack configuration for DEB
set(CPACK_DEBIAN_PACKAGE_NAME "network-manager-amneziawg")
set(CPACK_DEBIAN_PACKAGE_VERSION "${PROJECT_VERSION_FULL}")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "NetworkManager VPN plugin for AmneziaWG")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "network-manager (>= 1.0), libglib2.0-0")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

include(CPack)
set(CPACK_SET_DESTDIR ON)