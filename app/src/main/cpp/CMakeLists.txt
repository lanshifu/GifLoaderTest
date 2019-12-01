cmake_minimum_required(VERSION 3.4.1)

# 1、指定源文件目录，把 /cpp/giflib 目录的下的所有文件赋值给 GIF_LIB
file(GLOB_RECURSE GIF_LIB ${CMAKE_SOURCE_DIR}/giflib/*.*)
# 同1，cpp 目录下的文件文件放到 MAIN_SOURCE 里，不用一个一个添加
file(GLOB_RECURSE MAIN_SOURCE ${CMAKE_SOURCE_DIR}/*.*)


add_library(
        # 要编译的库的名称，可以改
        native-lib

        # SHARED 表示要编译动态库
        SHARED

        ${GIF_LIB}  # 2、把giflib源文件和native-lib.cpp添加到 native-lib 这个库中去
        ${MAIN_SOURCE}
)

target_link_libraries(
        native-lib

        # 3、给native-lib 添加一些依赖
        log
        jnigraphics
        android)