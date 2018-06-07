get_property(rtdir GLOBAL PROPERTY ROOT_DIR)

include_directories( SYSTEM
    ${DEPENDS_DIR}/include
    ${DEPENDS_DIR}/lib/libzip/include
    ${DEPENDS_DIR}/include/freetype2
    )

include_directories(
    ${rtdir}
    ${rtdir}/lib
    ${rtdir}/lib/addons/library.xbmc.addon
    ${rtdir}/xbmc
    ${rtdir}/xbmc/addons/include
    ${rtdir}/addons/library.kodi.guilib
    ${rtdir}/addons/library.xbmc.addon
    ${rtdir}/addons/library.kodi.adsp
    ${rtdir}/addons/library.kodi.audioengine
    ${rtdir}/addons/library.xbmc.pvr
    ${rtdir}/addons/library.xbmc.codec
    ${rtdir}/xbmc/linux
    ${rtdir}/xbmc/cores/dvdplayer
    )

