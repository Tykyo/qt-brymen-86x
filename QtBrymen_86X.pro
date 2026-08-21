TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS += \
    3rdparty/qwt/qwt_macos_patched.pro \
    app/Brymen_86X.pro

# ==============================================================================
# ==============================================================================
# deploy target
# ==============================================================================
# ==============================================================================
deploy.commands = @echo "=== DEPLOYMENT START ===" $$escape_expand(\\n\\t)
TARGET_NAME = BM86x

# ==============================================================================
# Windows Configuration (MinGW / MSVC)
# ==============================================================================
win32 {
    deploy.commands += @echo "--- Windows deployment ---" $$escape_expand(\\n\\t)
    LIB_DIR     = $$shell_path($$OUT_PWD/3rdparty/qwt/qwt/lib)
    TARGET_DLL  = $$shell_path($$LIB_DIR/qwt.dll)
    INSTALL_DIR = $$shell_path($$OUT_PWD/install)

    TARGET_EXE  = $$shell_path($$OUT_PWD/app/$${TARGET_NAME}.exe)

    # 1. Create the installation folder if it does not exist
    deploy.commands += @if not exist $$INSTALL_DIR mkdir $$INSTALL_DIR $$escape_expand(\\n\\t)
    # 2. Copy the built executable
    deploy.commands += copy /y $$TARGET_EXE $$INSTALL_DIR $$escape_expand(\\n\\t)
    # 3. Copy the Qwt library dependency
    deploy.commands += copy /y $$TARGET_DLL $$INSTALL_DIR $$escape_expand(\\n\\t)
    # 4. Run Qt's deployment tool to copy all required Qt DLLs automatically
    deploy.commands += windeployqt6 $$shell_path($$INSTALL_DIR/$${TARGET_NAME}.exe) $$escape_expand(\\n\\t)
}

# ==============================================================================
# Linux Configuration
# ==============================================================================
unix:!macx {
    deploy.commands += @echo "--- Linux deployment ---" $$escape_expand(\\n\\t)

    # Attention : no $$quote() or $$shell_path() here for TARGET_DLL because we use '*'
    TARGET_EXE  = $$shell_path($$OUT_PWD/app/$${TARGET_NAME})
    TARGET_DLL  = $$OUT_PWD/3rdparty/qwt/qwt/lib/libqwt.so*
    INSTALL_DIR = $$shell_path($$OUT_PWD/install)

    COPIED_EXE  = $$shell_path($$INSTALL_DIR/$${TARGET_NAME})

    # 1. Create the installation directory
    deploy.commands += @mkdir -p $$shell_path($$INSTALL_DIR/lib) $$escape_expand(\\n\\t)

    # 2. Copy the original executable (keeps its debug paths intact)
    deploy.commands += rsync -a $$TARGET_EXE $$INSTALL_DIR $$escape_expand(\\n\\t)

    # 3. Copy the qwt dependency libraries
    deploy.commands += rsync -a "$$TARGET_DLL" $$shell_path($$INSTALL_DIR/lib) $$escape_expand(\\n\\t)

    # 4. Copy the app icon (if you want to make a desktop shortcut)
    deploy.commands += rsync -a $$shell_path($$PWD/app/resources/main_icon.svg) $$INSTALL_DIR $$escape_expand(\\n\\t)

    # 5. install plugins, missing qt libraries and apply patchelf
    deploy.commands += bash $$shell_path($$PWD/deployment/linux/linux_install.sh) \
        $$shell_path($$[QT_INSTALL_PLUGINS]) \
        $$shell_path($$[QT_INSTALL_LIBS]) \
        $$INSTALL_DIR $${TARGET_NAME} $$escape_expand(\\n\\t)
}

# ==============================================================================
# macOS Configuration
# ==============================================================================
macx {
    deploy.commands += @echo "--- MacOS deployment ---" $$escape_expand(\\n\\t)
    BUILD_EXE   = $$shell_path($$OUT_PWD/app/$${TARGET_NAME}.app)
    INSTALL_DIR = $$shell_path($$OUT_PWD/install)
    TARGET_EXE  = $$BUILD_EXE

    CONFIG(no_bundle_qwt) {
        TARGET_EXE       = $$shell_path($$INSTALL_DIR/$${TARGET_NAME}.app)
        deploy.commands += @mkdir -p $$INSTALL_DIR $$escape_expand(\\n\\t)
        deploy.commands += @rsync -a $$BUILD_EXE $$INSTALL_DIR/ $$escape_expand(\\n\\t)
        deploy.commands += @echo "qwt.framework is bundled into the app" $$escape_expand(\\n\\t)
        LIB_DIR          = $$shell_path($$OUT_PWD/3rdparty/qwt/qwt/lib)
        FRAMEWORKS_DIR   = $$shell_path($$TARGET_EXE/Contents/Frameworks)
        deploy.commands += @mkdir -p $$FRAMEWORKS_DIR $$escape_expand(\\n\\t)
        deploy.commands += @rsync -a $$LIB_DIR/qwt.framework $$FRAMEWORKS_DIR/ $$escape_expand(\\n\\t)
    }

    # DMG bundle
    DEPLOY_SCRIPT = $$shell_path($$PWD/deployment/macx/bundle_dmg.sh)
    ENTITLEMENTS  = $$shell_path($$PWD/deployment/macx/USB.entitlements)
    exists($$DEPLOY_SCRIPT) {
        # Execute the custom bundle script if present in the environment
        deploy.commands += @echo "Deployment script found: $$DEPLOY_SCRIPT" $$escape_expand(\\n\\t)
        deploy.commands += bash $$DEPLOY_SCRIPT $$TARGET_EXE $$INSTALL_DIR $$ENTITLEMENTS $$escape_expand(\\n\\t)
    }
    else {
        deploy.commands += @echo -e "\033[0;31m""Deployment script not found: \
        $$DEPLOY_SCRIPT. Cannot deploy $$TARGET_EXE." "\033[0m" $$escape_expand(\\n\\t)
        deploy.commands += @false $$escape_expand(\\n\\t)
    }

    # Clean temp file
    CONFIG(no_bundle_qwt) {
        deploy.commands += rm -r $$TARGET_EXE $$escape_expand(\\n\\t)
    }
}

deploy.commands += @echo "=== DEPLOYMENT END ===" $$escape_expand(\\n\\t)
# ==============================================================================
# END Deploy target
# ==============================================================================
QMAKE_EXTRA_TARGETS += deploy