cmake_minimum_required(VERSION 3.22)
project(darkcode_scene LANGUAGES CXX)
option(DARKCODE_PANEL_ONLY "Build only the login and presentation panel" OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(scene_core STATIC
    core/frame_snapshot.cpp projection/projection.cpp adapters/normalize.cpp
    sources/mock_source.cpp sources/replay_source.cpp runtime/frame_runtime.cpp
    renderer/scene_geometry.cpp)
target_include_directories(scene_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(scene_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(scene_core PRIVATE -Wall -Wextra -Wpedantic)

# ── Módulo ESP (lector de memoria + proyección de objetivos) ──
# Independiente de la escena: si no se usa, no cuesta nada. driver_linux.cpp
# y process_linux.cpp se compilan también en escritorio (stubs sin ioctl) para
# que las pruebas y el CI de Linux funcionen sin dispositivo.
if(NOT DARKCODE_PANEL_ONLY)
add_library(esp_core STATIC
    esp/types.cpp
    esp/driver_linux.cpp
    esp/process_linux.cpp
    esp/elf.cpp
    esp/scanner.cpp
    esp/ue.cpp
    esp/reader.cpp
    esp/projector.cpp
    esp/api/esp_service.cpp
    esp/api/esp_api.cpp)
target_link_libraries(esp_core PUBLIC scene_core)
target_include_directories(esp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/esp)
set_target_properties(esp_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(esp_core PRIVATE -Wall -Wextra -Wpedantic)

# Herramienta de diagnóstico (esp_df). Apagada por defecto: Gradle no la
# necesita. Se enciende para compilar el binario que se copia al teléfono:
#   cmake -S native -B out -DDARKCODE_ESP_TOOL=ON ...
option(DARKCODE_ESP_TOOL "Compilar la herramienta de diagnostico esp_df" OFF)

if(DARKCODE_ESP_TOOL)
    add_executable(esp_df esp/tool/main.cpp)
    target_link_libraries(esp_df PRIVATE esp_core)
    if(ANDROID)
        target_link_options(esp_df PRIVATE "-Wl,-z,max-page-size=16384")
    endif()
endif()

endif()

if(ANDROID)
    add_library(darkcode_scene SHARED bridge/jni_bridge.cpp renderer/opengl_es/gl_renderer.cpp)
    target_link_libraries(darkcode_scene PRIVATE scene_core GLESv2 log)
    if(NOT DARKCODE_PANEL_ONLY)
        target_sources(darkcode_scene PRIVATE esp/esp_jni.cpp)
        target_link_libraries(darkcode_scene PRIVATE esp_core)
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_sources(darkcode_scene PRIVATE tests/core_tests.cpp)
        target_compile_definitions(darkcode_scene PRIVATE DARKCODE_TESTING)
    endif()
    target_link_options(darkcode_scene PRIVATE "-Wl,-z,max-page-size=16384")
else()
    find_package(Threads REQUIRED)
    target_link_libraries(scene_core PUBLIC Threads::Threads)
    enable_testing()
    add_executable(core_tests tests/core_tests.cpp tests/main.cpp)
    target_link_libraries(core_tests PRIVATE scene_core)
    add_test(NAME scene_contracts COMMAND core_tests)
    if(NOT DARKCODE_PANEL_ONLY)
        add_executable(esp_tests esp/tests/esp_tests.cpp esp/tests/main.cpp)
        target_link_libraries(esp_tests PRIVATE esp_core)
        add_test(NAME esp_contracts COMMAND esp_tests)
    endif()
endif()
