cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
        WinEventsAndroid.cpp
        WinSystemAndroidEGL.cpp
        )

add_library(
        windowing_android
        STATIC

        ${my_SOURCES}
        )
