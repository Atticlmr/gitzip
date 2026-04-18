# Copyright (C) 2026 gitzip contributors
#
# This file is part of gitzip.
#
# gitzip is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# gitzip is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with gitzip.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.16)

project(gitzip VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LZMA_SDK_CORE_LIB ${CMAKE_BINARY_DIR}/third_party/lib7zr_core.a)

add_custom_command(
    OUTPUT ${LZMA_SDK_CORE_LIB}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/third_party
    COMMAND /usr/bin/env bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_lzma_sdk_core.sh
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${LZMA_SDK_CORE_LIB}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_lzma_sdk_core.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
)

add_custom_target(lzma_sdk_core ALL DEPENDS ${LZMA_SDK_CORE_LIB})

add_executable(gitzip
    src/gitignore.cpp
    src/main.cpp
)

add_dependencies(gitzip lzma_sdk_core)

target_compile_definitions(gitzip
    PRIVATE
        GITZIP_VERSION="${PROJECT_VERSION}"
        GITZIP_HOME_PAGE="https://github.com/Atticlmr/gitzip"
        GITZIP_BUG_REPORTS="https://github.com/Atticlmr/gitzip/issues"
)

target_include_directories(gitzip
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lzma-sdk-26.00/CPP
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lzma-sdk-26.00/C
)

if(MSVC)
    target_compile_options(gitzip PRIVATE /W4 /permissive-)
else()
    target_compile_options(gitzip PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(gitzip
    PRIVATE
        dl
        pthread
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(gitzip
        PRIVATE
            -Wl,--whole-archive
            ${LZMA_SDK_CORE_LIB}
            -Wl,--no-whole-archive
    )
else()
    target_link_libraries(gitzip PRIVATE ${LZMA_SDK_CORE_LIB})
endif()

install(TARGETS gitzip
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    ${CMAKE_CURRENT_SOURCE_DIR}/COPYING
    ${CMAKE_CURRENT_SOURCE_DIR}/NOTICE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/gitzip
)
