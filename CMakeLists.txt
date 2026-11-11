cmake_minimum_required(VERSION 3.20)
project(AstroBotPC VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------------------------------------------------------------------------
# Configurações de Build
# -------------------------------------------------------------------------
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /GL /DNDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG")
    add_compile_options(/W4 /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

# -------------------------------------------------------------------------
# Decoder ATRAC9 (LibAtrac9, MIT)
# -------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    libatrac9
    GIT_REPOSITORY https://github.com/Thealexbarney/LibAtrac9.git
    GIT_TAG efca2e3af35562a09a9bb6deed90e45b4b824dc4
)
FetchContent_MakeAvailable(libatrac9)

file(GLOB ATRAC9_SOURCES CONFIGURE_DEPENDS
    "${libatrac9_SOURCE_DIR}/C/src/*.c"
)
add_library(Atrac9 STATIC ${ATRAC9_SOURCES})
target_include_directories(Atrac9 PUBLIC "${libatrac9_SOURCE_DIR}/C/src")
target_compile_definitions(Atrac9 PRIVATE COMPILING_DLL)

# -------------------------------------------------------------------------
# Bibliotecas comuns
# -------------------------------------------------------------------------
add_library(AstroBotCore STATIC
    src/config/config_manager.cpp
    src/patches/patch_manager.cpp
    src/kernel/kernel_emulator.cpp
    src/gpu/vulkan_backend.cpp
    src/gpu/vulkan_loader.cpp
    src/gpu/shader_recompiler.cpp
    src/audio/audio_backend.cpp
    src/stubs/sce_audiodec.cpp
    src/input/input_manager.cpp
    src/core/eboot_loader.cpp
    src/core/nid_resolver.cpp
    src/core/dispatch_table.cpp
    src/assets/asset_loader.cpp
    src/assets/jxmd_parser.cpp
    src/assets/asset_census.cpp
    src/assets/gnf_format.cpp
    src/gameplay/level_collision.cpp
    src/common/types.h
    src/stubs/sce_stubs.h
)

target_include_directories(AstroBotCore PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(AstroBotCore PRIVATE Atrac9)

# -------------------------------------------------------------------------
# Executáveis
# -------------------------------------------------------------------------
add_executable(AstroBotPC WIN32 src/main.cpp)
target_link_libraries(AstroBotPC PRIVATE AstroBotCore)
target_include_directories(AstroBotPC PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(benchmark_load src/benchmark_load.cpp)
target_link_libraries(benchmark_load PRIVATE AstroBotCore)
target_include_directories(benchmark_load PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(test_pipeline src/test_pipeline.cpp)
target_link_libraries(test_pipeline PRIVATE AstroBotCore)
target_include_directories(test_pipeline PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(test_eboot_load src/test_eboot_load.cpp)
target_link_libraries(test_eboot_load PRIVATE AstroBotCore)
target_include_directories(test_eboot_load PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(test_elf_metadata src/test_elf_metadata.cpp)
target_link_libraries(test_elf_metadata PRIVATE AstroBotCore)
target_include_directories(test_elf_metadata PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME elf_metadata_validation COMMAND test_elf_metadata)

add_executable(test_self_inspection src/test_self_inspection.cpp)
target_link_libraries(test_self_inspection PRIVATE AstroBotCore)
target_include_directories(test_self_inspection PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME self_inspection_validation COMMAND test_self_inspection)

add_executable(test_self_reconstruction src/test_self_reconstruction.cpp)
target_link_libraries(test_self_reconstruction PRIVATE AstroBotCore)
target_include_directories(test_self_reconstruction PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME self_reconstruction_validation COMMAND test_self_reconstruction)

add_executable(diag_eboot src/diag_eboot.cpp)
target_link_libraries(diag_eboot PRIVATE AstroBotCore)
target_include_directories(diag_eboot PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(diag_assets src/diag_assets.cpp)
target_link_libraries(diag_assets PRIVATE AstroBotCore)
target_include_directories(diag_assets PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(diag_gnf src/diag_gnf.cpp)
target_link_libraries(diag_gnf PRIVATE AstroBotCore)
target_include_directories(diag_gnf PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(test_tex_load src/test_tex_load.cpp)
target_link_libraries(test_tex_load PRIVATE AstroBotCore)
target_include_directories(test_tex_load PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(test_asset_census src/test_asset_census.cpp)
target_link_libraries(test_asset_census PRIVATE AstroBotCore)
target_include_directories(test_asset_census PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME asset_census_validation COMMAND test_asset_census)

add_executable(diag_census src/diag_census.cpp)
target_link_libraries(diag_census PRIVATE AstroBotCore)
target_include_directories(diag_census PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(test_audio_at9 src/test_audio_at9.cpp)
target_link_libraries(test_audio_at9 PRIVATE AstroBotCore)
target_include_directories(test_audio_at9 PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME at9_decoder_validation COMMAND test_audio_at9)

add_executable(test_gameplay_movement src/test_gameplay_movement.cpp)
target_include_directories(test_gameplay_movement PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME gameplay_movement_validation COMMAND test_gameplay_movement)

add_executable(test_level_collision src/test_level_collision.cpp)
target_link_libraries(test_level_collision PRIVATE AstroBotCore)
target_include_directories(test_level_collision PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME level_collision_validation COMMAND test_level_collision)

add_executable(test_software_rasterizer src/test_software_rasterizer.cpp)
target_include_directories(test_software_rasterizer PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME software_rasterizer_validation COMMAND test_software_rasterizer)

add_executable(benchmark_software_rasterizer src/benchmark_software_rasterizer.cpp)
target_include_directories(benchmark_software_rasterizer PRIVATE ${CMAKE_SOURCE_DIR}/src)

enable_testing()

if(WIN32)
    set(ALL_TARGETS
        AstroBotPC
        benchmark_load
        test_pipeline
        test_eboot_load
        test_elf_metadata
        test_self_inspection
        test_self_reconstruction
        diag_eboot
        diag_assets
        diag_gnf
        diag_census
        test_tex_load
        test_asset_census
        test_audio_at9
        test_gameplay_movement
        test_level_collision
        test_software_rasterizer
        benchmark_software_rasterizer
    )
    foreach(tgt ${ALL_TARGETS})
        target_link_libraries(${tgt} PRIVATE
            xinput.lib
            kernel32.lib
            user32.lib
            gdi32.lib
            shell32.lib
            ole32.lib
            mmdevapi.lib
        )
        target_compile_definitions(${tgt} PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            UNICODE
            _UNICODE
        )
    endforeach()
endif()

add_custom_command(TARGET AstroBotPC POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:AstroBotPC>/config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/config/settings.ini
        $<TARGET_FILE_DIR:AstroBotPC>/config/settings.ini
    COMMENT "Copiando settings.ini para diretório de output..."
)
