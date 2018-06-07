cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

include_directories ( ${DEPENDS_DIR}/include/python2.7 )

set (my_SOURCES
        HTTPPythonInvoker.cpp
        HTTPPythonWsgiInvoker.cpp
        )

add_library(
        httprequesthandlers-python
        STATIC

        ${my_SOURCES}
        )
