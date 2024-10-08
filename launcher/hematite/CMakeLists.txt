
# SPDX-FileCopyrightText: 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
#
# SPDX-License-Identifier: GPL-3.0-only
#
#  Prism Launcher - Minecraft Launcher
#  Copyright (C) 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, version 3.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.

# Rust always links against non-debug Windows runtime on *-msvc targets
# Note it is best to set this on the command line to ensure all targets are consistent
# https://github.com/corrosion-rs/corrosion/blob/master/doc/src/common_issues.md#linking-debug-cc-libraries-into-rust-fails-on-windows-msvc-targets
# https://github.com/rust-lang/rust/issues/39016
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

find_package(CxxQt QUIET)
if(NOT CxxQt_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        CxxQt
        GIT_REPOSITORY https://github.com/kdab/cxx-qt-cmake.git
        GIT_TAG main
    )

    FetchContent_MakeAvailable(CxxQt)
endif()

function(LinkCxxQtCrate CRATE_DIR CRATE)
  cxx_qt_import_crate(MANIFEST_PATH "${CRATE_DIR}/Cargo.toml" CRATES ${CRATE})


  target_link_libraries(${CRATE} INTERFACE Qt::Core)

  # if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${CRATE}/qml" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${CRATE}/qml")
  # 
  #   file(GLOB build_cxx_qt_qml_files_list "${CMAKE_CURRENT_SOURCE_DIR}/${CRATE}/qml/*")
  #   list(LENGTH build_cxx_qt_qml_files_list build_cxx_qt_qml_files_list_count)
  #   if(build_cxx_qt_qml_files_list_count GREATER 0)
  #
  #     target_link_libraries(${CRATE} INTERFACE Qt::Gui Qt::Qml Qt::QuickControls2)
  #
  #     cxx_qt_import_qml_module(${CRATE}_qml
  #       URI "org.prismlauncher.hematite.${CRATE}"
  #       SOURCE_CRATE ${CRATE}
  #     )
  #
  #     target_link_libraries(Launcher_logic PRIVATE ${CRATE}_qml)
  #   endif()
  # endif()

  target_link_libraries(Launcher_logic INTERFACE ${CRATE})
endfunction()

# Automated instertion marker for `cargo xtask new`
# BEGIN CRATES_TO_BUILD
# END CRATES_TO BUILD
