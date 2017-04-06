SUMMARY = "transparent controls demo"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "git://github.com/ssalenik/transparent-controls.git;rev=fc44eeda5a44a92f1070430c5e46c8bdaccb6a8d"

S = "${WORKDIR}/git"

DEPENDS = "gstreamer1.0"

inherit qmake5

OE_QMAKE_CXXFLAGS += "\
    -I${STAGING_KERNEL_DIR}/include/uapi \
    -I${STAGING_KERNEL_DIR}/include \
"

do_install() {
    install -d ${D}${datadir}/${P}
    install -m 0755 ${B}/transparent ${D}${datadir}/${P}
    install -m 0644 ${S}/transparent.qml ${D}${datadir}/${P}

    install -d ${D}${bindir}
    echo "#!/bin/sh" > ${D}${bindir}/transparent-controls
    echo "export QML_IMPORT_PATH=${datadir}/${P}" >> ${D}${bindir}/transparent-controls
    echo "export QML2_IMPORT_PATH=${datadir}/${P}" >> ${D}${bindir}/transparent-controls
    echo "${datadir}/${P}/transparent \$* " >> ${D}${bindir}/transparent-controls
    chmod +x ${D}${bindir}/transparent-controls
}

FILES_${PN} += "${datadir}"

