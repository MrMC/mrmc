cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

#add_subdirectory (${rtdir}/../libandroidjni ${lib_build_DIR}/androidjni)

add_library(androidjni STATIC IMPORTED)
set_target_properties(androidjni PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libandroidjni.a)

add_library(icundk STATIC IMPORTED)
set_target_properties(icundk PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libicundk.a)

add_library(smbclient SHARED IMPORTED)
set_target_properties(smbclient PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libsmbclient.so)

add_library(ssh STATIC IMPORTED)
set_target_properties(ssh PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libssh.a)

add_library(mDNSEmbedded STATIC IMPORTED)
set_target_properties(mDNSEmbedded PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libmDNSEmbedded.a)

add_library(microhttpd STATIC IMPORTED)
set_target_properties(microhttpd PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libmicrohttpd.a)

add_library(iconv STATIC IMPORTED)
set_target_properties(iconv PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libiconv.a)

add_library(mysqlclient STATIC IMPORTED)
set_target_properties(mysqlclient PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/mysql/libmysqlclient.a)

add_library(ssl STATIC IMPORTED)
set_target_properties(ssl PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libssl.a)

add_library(crypto STATIC IMPORTED)
set_target_properties(crypto PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libcrypto.a)

add_library(lzo2 STATIC IMPORTED)
set_target_properties(lzo2 PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/liblzo2.a)

add_library(ulxmlrpcpp STATIC IMPORTED)
set_target_properties(ulxmlrpcpp PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libulxmlrpcpp.a)

add_library(expat STATIC IMPORTED)
set_target_properties(expat PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libexpat.a)

add_library(jpeg STATIC IMPORTED)
set_target_properties(jpeg PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libjpeg.a)

add_library(gif STATIC IMPORTED)
set_target_properties(gif PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libgif.a)

add_library(bz2 STATIC IMPORTED)
set_target_properties(bz2 PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libbz2.a)

add_library(gcrypt STATIC IMPORTED)
set_target_properties(gcrypt PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libgcrypt.a)

add_library(gpg-error STATIC IMPORTED)
set_target_properties(gpg-error PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libgpg-error.a)

add_library(python2.7 STATIC IMPORTED)
set_target_properties(python2.7 PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libpython2.7.a)

add_library(ffi STATIC IMPORTED)
set_target_properties(ffi PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libffi.a)

add_library(tinyxml STATIC IMPORTED)
set_target_properties(tinyxml PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libtinyxml.a)

add_library(crossguid STATIC IMPORTED)
set_target_properties(crossguid PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libcrossguid.a)

add_library(uuid STATIC IMPORTED)
set_target_properties(uuid PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libuuid.a)

add_library(xml2 STATIC IMPORTED)
set_target_properties(xml2 PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libxml2.a)

add_library(xslt STATIC IMPORTED)
set_target_properties(xslt PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libxslt.a)

add_library(fribidi STATIC IMPORTED)
set_target_properties(fribidi PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libfribidi.a)

add_library(sqlite3 STATIC IMPORTED)
set_target_properties(sqlite3 PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libsqlite3.a)

add_library(png16 STATIC IMPORTED)
set_target_properties(png16 PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libpng16.a)

add_library(pcre STATIC IMPORTED)
set_target_properties(pcre PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libpcre.a)

add_library(pcrecpp STATIC IMPORTED)
set_target_properties(pcrecpp PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libpcrecpp.a)

add_library(freetype STATIC IMPORTED)
set_target_properties(freetype PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libfreetype.a)

add_library(tag STATIC IMPORTED)
set_target_properties(tag PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libtag.a)

add_library(zip STATIC IMPORTED)
set_target_properties(zip PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libzip.a)

add_library(avfilter STATIC IMPORTED)
set_target_properties(avfilter PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libavfilter.a)

add_library(avformat STATIC IMPORTED)
set_target_properties(avformat PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libavformat.a)

add_library(avcodec STATIC IMPORTED)
set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libavcodec.a)

add_library(gnutls STATIC IMPORTED)
set_target_properties(gnutls PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libgnutls.a)

add_library(gmp STATIC IMPORTED)
set_target_properties(gmp PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libgmp.a)

add_library(hogweed STATIC IMPORTED)
set_target_properties(hogweed PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libhogweed.a)

add_library(nettle STATIC IMPORTED)
set_target_properties(nettle PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libnettle.a)

add_library(tasn1 STATIC IMPORTED)
set_target_properties(tasn1 PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libtasn1.a)

add_library(postproc STATIC IMPORTED)
set_target_properties(postproc PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libpostproc.a)

add_library(swscale STATIC IMPORTED)
set_target_properties(swscale PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libswscale.a)

add_library(swresample STATIC IMPORTED)
set_target_properties(swresample PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libswresample.a)

add_library(avutil STATIC IMPORTED)
set_target_properties(avutil PROPERTIES IMPORTED_LOCATION
    ${DEPENDS_DIR}/lib/libavutil.a)

