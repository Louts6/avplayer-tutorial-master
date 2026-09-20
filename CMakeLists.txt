cmake_minimum_required(VERSION 3.22.1)
project(LearnAV VERSION 0.0.0 LANGUAGES C CXX)

# -----------------------------------------------------------------------------
# 公共：C/C++ 标准与路径
# -----------------------------------------------------------------------------
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 项目根目录（Android Gradle 已指向本文件，故 source dir 即为仓库根）
set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# -----------------------------------------------------------------------------
# Android 构建：共享库 learnav，供 JNI 使用
# -----------------------------------------------------------------------------
if(ANDROID)
    # FFmpeg 库路径
    set(FFMPEG_DIR "${PROJECT_ROOT_DIR}/3rdparty/FFmpeg/Android")
    set(FFMPEG_LIB_DIR "${FFMPEG_DIR}/lib")
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(FFMPEG_INCLUDE_DIR "${FFMPEG_DIR}/include/arm64-v8a")
        set(FFMPEG_LIB "${FFMPEG_LIB_DIR}/arm64-v8a/libijkffmpeg.so")
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(FFMPEG_INCLUDE_DIR "${FFMPEG_DIR}/include/armeabi-v7a")
        set(FFMPEG_LIB "${FFMPEG_LIB_DIR}/armeabi-v7a/libijkffmpeg.so")
    else()
        message(FATAL_ERROR "Unsupported ABI: ${ANDROID_ABI}")
    endif()

    if(NOT EXISTS "${FFMPEG_LIB}")
        message(FATAL_ERROR "FFmpeg library not found: ${FFMPEG_LIB}")
    endif()
    message(STATUS "Using FFmpeg library: ${FFMPEG_LIB}")

    # 头文件
    include_directories(
        ${PROJECT_ROOT_DIR}/LearnAV/include
        ${PROJECT_ROOT_DIR}/LearnAV/src
        ${PROJECT_ROOT_DIR}/3rdparty/stb
        ${PROJECT_ROOT_DIR}/3rdparty/glm
        ${FFMPEG_INCLUDE_DIR}
    )

    # 核心源文件（按目录显式收集，便于排除平台差异）
    file(GLOB_RECURSE CORE_SOURCES
        "${PROJECT_ROOT_DIR}/LearnAV/src/Core/*.cpp"
        "${PROJECT_ROOT_DIR}/LearnAV/src/Define/*.cpp"
        "${PROJECT_ROOT_DIR}/LearnAV/src/Engine/*.cpp"
        "${PROJECT_ROOT_DIR}/LearnAV/src/Interface/*.cpp"
        "${PROJECT_ROOT_DIR}/LearnAV/src/Reader/*.cpp"
        "${PROJECT_ROOT_DIR}/LearnAV/src/Utils/*.cpp"
        "${PROJECT_ROOT_DIR}/LearnAV/src/VideoFilter/*.cpp"
        "${PROJECT_ROOT_DIR}/LearnAV/src/Writer/*.cpp"
    )
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*StickerFilter\\.cpp$")
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/Engine/VideoDisplayView\\.cpp$")
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/Engine/AudioSpeaker\\.cpp$")
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/Engine/GLContext\\.cpp$")

    file(GLOB ANDROID_SOURCES "${PROJECT_ROOT_DIR}/LearnAV/src/Android/*.cpp")

    add_library(learnav SHARED ${CORE_SOURCES} ${ANDROID_SOURCES})

    add_library(ijkffmpeg SHARED IMPORTED)
    set_target_properties(ijkffmpeg PROPERTIES IMPORTED_LOCATION "${FFMPEG_LIB}")

    target_link_libraries(learnav ijkffmpeg GLESv3 EGL OpenSLES android log)
    target_compile_definitions(learnav PRIVATE ANDROID)
    target_include_directories(learnav PRIVATE
        ${PROJECT_ROOT_DIR}/LearnAV/include
        ${PROJECT_ROOT_DIR}/LearnAV/src
        ${PROJECT_ROOT_DIR}/3rdparty/stb
        ${PROJECT_ROOT_DIR}/3rdparty/glm
        ${FFMPEG_INCLUDE_DIR}
    )

# -----------------------------------------------------------------------------
# 桌面构建：静态库 LearnAV + QtDemo
# -----------------------------------------------------------------------------
else()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_definitions(DEBUG)
    endif()

    # CCache
    find_program(CCACHE_FOUND ccache)
    if(CCACHE_FOUND)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    endif()

    set(RESOURCE_DIR "${PROJECT_ROOT_DIR}/resource")
    set(CACHE_DIR "${PROJECT_ROOT_DIR}/cache")
    add_compile_definitions(
        RESOURCE_DIR="${RESOURCE_DIR}"
        CACHE_DIR="${CACHE_DIR}"
    )

    if(WIN32)
        # Windows: use prebuilt FFmpeg shared libraries from 3rdparty
        set(FFMPEG_WIN_DIR "${PROJECT_ROOT_DIR}/3rdparty/FFmpeg/Windows")
        set(FFMPEG_INCLUDE_DIR "${FFMPEG_WIN_DIR}/include")
        set(FFMPEG_LIB_DIR "${FFMPEG_WIN_DIR}/lib")
        set(STB_INCLUDE_DIR "${PROJECT_ROOT_DIR}/3rdparty/stb")
        set(GLM_INCLUDE_DIR "${PROJECT_ROOT_DIR}/3rdparty/glm")
        set(GLAD_INCLUDE_DIR "${PROJECT_ROOT_DIR}/3rdparty/glad/include")
        set(GLAD_SRC "${PROJECT_ROOT_DIR}/3rdparty/glad/src/glad.c")
        find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Quick)
    else()
        find_package(ffmpeg REQUIRED)
        find_package(stb REQUIRED)
        find_package(glm REQUIRED)
        find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Multimedia Quick)
    endif()

    set(INSPIREFACE_BASE_DIR "${PROJECT_ROOT_DIR}/3rdparty/InspireFace")
    if(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(INSPIREFACE_ARCH "arm64")
        else()
            set(INSPIREFACE_ARCH "x86_64")
        endif()
        set(INSPIREFACE_LIB_FILE "${INSPIREFACE_BASE_DIR}-${INSPIREFACE_ARCH}/lib/libInspireFace.dylib")
        set(INSPIREFACE_INCLUDE_DIR "${INSPIREFACE_BASE_DIR}-${INSPIREFACE_ARCH}/include")
    else()
        set(INSPIREFACE_LIB_FILE "${INSPIREFACE_BASE_DIR}/lib/libInspireFace.dylib")
        set(INSPIREFACE_INCLUDE_DIR "${INSPIREFACE_BASE_DIR}/include")
    endif()

    set(INCLUDE_DIRS
        ${PROJECT_ROOT_DIR}/LearnAV/include
        ${PROJECT_ROOT_DIR}/LearnAV/src
        ${Qt6_INCLUDES}
        ${Qt6OpenGL_INCLUDE_DIRS}
        ${Qt6OpenGLWidgets_INCLUDE_DIRS}
        ${Qt6Multimedia_INCLUDE_DIRS}
    )
    if(WIN32)
        list(APPEND INCLUDE_DIRS ${FFMPEG_INCLUDE_DIR} ${STB_INCLUDE_DIR} ${GLM_INCLUDE_DIR} ${GLAD_INCLUDE_DIR})
    endif()

    file(GLOB_RECURSE LIBRARY_SOURCE_FILES ${PROJECT_ROOT_DIR}/LearnAV/*.cpp)
    list(FILTER LIBRARY_SOURCE_FILES EXCLUDE REGEX ".*/Android/.*\\.cpp$")
    if(WIN32)
        list(FILTER LIBRARY_SOURCE_FILES EXCLUDE REGEX ".*StickerFilter\\.cpp$")
        list(FILTER LIBRARY_SOURCE_FILES EXCLUDE REGEX ".*/Engine/AudioSpeaker\\.cpp$")
    endif()

    add_library(${PROJECT_NAME} STATIC ${LIBRARY_SOURCE_FILES} $<${WIN32}:${GLAD_SRC}>)
    if(WIN32)
        target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_DIRS})
        target_link_directories(${PROJECT_NAME} PUBLIC ${FFMPEG_LIB_DIR})
        target_link_libraries(${PROJECT_NAME} PUBLIC
            Qt6::Core Qt6::Gui Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Widgets
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE
            avcodec avformat avutil swscale swresample avfilter avdevice
            ws2_32 user32 gdi32 ole32 strmiids
        )
    else()
        target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_DIRS} ${INSPIREFACE_INCLUDE_DIR})
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ffmpeg::ffmpeg stb::stb glm::glm ${INSPIREFACE_LIB_FILE}
            Qt6::Core Qt6::Multimedia
        )
    endif()

    file(GLOB_RECURSE DEMO_SOURCE_FILES ${PROJECT_ROOT_DIR}/qt/*.cpp)
    add_executable(QtDemo ${DEMO_SOURCE_FILES})
    target_include_directories(QtDemo PRIVATE ${PROJECT_ROOT_DIR}/qt ${PROJECT_ROOT_DIR}/LearnAV/qt)
    if(WIN32)
        target_link_directories(QtDemo PRIVATE ${FFMPEG_LIB_DIR})
        target_link_libraries(QtDemo PRIVATE
            Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets
            Qt6::Quick ${PROJECT_NAME}
            avcodec avformat avutil swscale swresample avfilter avdevice
        )
        # Copy FFmpeg DLLs to output directory
        add_custom_command(TARGET QtDemo POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${FFMPEG_WIN_DIR}/bin" "$<TARGET_FILE_DIR:QtDemo>"
        )
    else()
        target_link_libraries(QtDemo PRIVATE
            Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets
            Qt6::Multimedia Qt6::Quick ${PROJECT_NAME}
        )
    endif()
endif()
