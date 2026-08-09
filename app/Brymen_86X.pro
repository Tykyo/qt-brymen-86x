QT += widgets core gui svg serialport printsupport network concurrent openglwidgets

CONFIG += c++20

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

TARGET = "BM86x"
DEFINES += _TARGET=\\\"$${TARGET}\\\"
DEFINES += SERIAL_CONNECT_DELAY=2000 # Can be decrease to 50 if not using an arduino board
DEFINES += BM_TCP_PORT=3333 # Use the same as defined in the firmware

SOURCES += \
    bm86xqwtplot.cpp \
    datastorage.cpp \
    dialogconnect.cpp \
    main.cpp \
    bm86xgui.cpp

HEADERS += \
    barscale.h \
    bm86xgui.h \
    bm86xplot.h \
    bm86xqwtplot.h \
    datastorage.h \
    dialogconnect.h \
    dualaxiszoomer.h \
    helpwindow.h \
    shortcutaction.h

FORMS += \
    bm86xgui.ui \
    datastorage.ui \
    dialogconnect.ui

RESOURCES += \
    bm86xgui.qrc

include(../3rdparty/miniaudio.pri)
include(../3rdparty/QXlsx/QXlsx/QXlsx.pri)
include(../firmware/lib/Brymen/bm86xdecode.pri)

# ==============================================================================
# Windows Configuration (MinGW / MSVC)
# ==============================================================================
win32 {
    DEFINES += QWT_DLL
    # Set the application icon for Windows Explorer
    RC_ICONS = resources/main_icon.ico

    # Define and clean paths
    LIB_DIR     = $$shell_path($$OUT_PWD/../3rdparty/qwt/qwt/lib)
    INCLUDE_DIR = $$shell_path($$PWD/../3rdparty/qwt/qwt/src)

    # Link against Qwt library depending on the build type (Release vs Debug)
    CONFIG(release, debug|release) {
        LIBS += -L"$$LIB_DIR" -lqwt
    } else {
        LIBS += -L"$$LIB_DIR" -lqwtd
    }

    CONFIG -= debug_and_release
    CONFIG -= build_all

    # Include headers for the Qwt library
    INCLUDEPATH += $$INCLUDE_DIR
}

# ==============================================================================
# Linux Configuration
# ==============================================================================
unix:!macx {
    # Define and clean paths
    LIB_DIR     = $$shell_path($$OUT_PWD/../3rdparty/qwt/qwt/lib)
    INCLUDE_DIR = $$shell_path($$PWD/../3rdparty/qwt/qwt/src)

    CONFIG      += link_pkgconfig
    INCLUDEPATH += $$INCLUDE_DIR
    LIBS        += -L"$$LIB_DIR" -lqwt

    CONFIG(debug, debug|release) {
        QMAKE_CXXFLAGS += \
            -fsanitize=address \
            -fno-omit-frame-pointer \
            -g

        QMAKE_LFLAGS += \
            -fsanitize=address
    }
}

# ==============================================================================
# macOS Configuration
# ==============================================================================
macx {
    # Skip SDK version checks to prevent warnings on newer macOS releases
    CONFIG += sdk_no_version_check

    # Set the application icon (.icns format is required on macOS)
    ICON = resources/main_icon.icns

    # Define and clean paths
    LIB_DIR     = $$shell_path($$OUT_PWD/../3rdparty/qwt/qwt/lib)
    INCLUDE_DIR = $$shell_path($$OUT_PWD/../3rdparty/qwt/qwt/lib/qwt.framework/Headers)
    TARGET_EXE  = $$shell_path($$OUT_PWD/$${TARGET}.app)

    # Include headers from the built Qwt framework bundle
    INCLUDEPATH += $$INCLUDE_DIR

    # Link against Qwt as a macOS Framework
    LIBS         += -F"$$LIB_DIR" -framework qwt
    QMAKE_LFLAGS += -Wl,-F"$$LIB_DIR" -Wl,-framework,qwt

    !CONFIG(no_bundle_qwt) {
        message("qwt.framework is bundled into the app")
        FRAMEWORKS_DIR = $$shell_path($$OUT_PWD/$${TARGET}.app/Contents/Frameworks)
        QMAKE_POST_LINK += mkdir -p $$FRAMEWORKS_DIR $$escape_expand(\\n\\t)
        QMAKE_POST_LINK += rsync -a $$LIB_DIR/qwt.framework $$FRAMEWORKS_DIR/ $$escape_expand(\\n\\t)
    } else {
        !exists($$shell_path($$[QT_INSTALL_LIBS]/qwt.framework)) {
            CONFIG(install_qwt) {
                QMAKE_POST_LINK += @echo "=== QWT INSTALLATION START ===" $$escape_expand(\\n\\t)
                QMAKE_POST_LINK += rsync -a $$shell_path($$LIB_DIR/qwt.framework) $$shell_path($$[QT_INSTALL_LIBS]/) $$escape_expand(\\n\\t)
                QMAKE_POST_LINK += @echo "=== QWT INSTALLATION ===" $$escape_expand(\\n\\t)
            }
            else {
                message("qwt.framework is not bundled into the app, she can failed to run")
                message("qwt.framework can be installed by setting CONFIG+=install_qwt")
                message("qwt.framework can also be copied manualy to $$[QT_INSTALL_LIBS]")
            }
        }
    }

    # Path to custom macOS codesigning/packaging script
    CODESIGN_SCRIPT = $$shell_path($$PWD/../deployment/macx/codesign.sh)

    # Handle code signing
    exists($$CODESIGN_SCRIPT) {
        # Execute the custom code signing script if present in the environment
        message("Signing script found: $$CODESIGN_SCRIPT")
        QMAKE_POST_LINK += bash $$CODESIGN_SCRIPT $$TARGET_EXE $$escape_expand(\\n\\t)
    } else {
        # Fallback: self-sign the bundle locally if the dedicated script is missing
        message("Signing script not found: $$CODESIGN_SCRIPT. Performing local ad-hoc signing.")
        QMAKE_POST_LINK += codesign --force -s - $$TARGET_EXE $$escape_expand(\\n\\t)
    }
}

# Check Qwt version
QWT_GLOBAL_FILE = $$shell_path($$PWD/../3rdparty/qwt/qwt/src/qwt_global.h)

!exists($$QWT_GLOBAL_FILE) {
    error("$$QWT_GLOBAL_FILE not found!")
}

QWT_GLOBAL = $$cat($$QWT_GLOBAL_FILE)

found = false
previous = ""

for(token, QWT_GLOBAL) {
    !found {
        equals(previous, "QWT_VERSION") {
            equals(token, "0x060300") {
                found = true
            }
        }
        previous = $$token
    }
}

!equals(found, true) {
    error("Wrong Qwt version. Expected 6.3.0 (0x060300). Did you update the submodule?")
}
