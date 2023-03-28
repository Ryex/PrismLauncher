# Standard FIND_PACKAGE module for libaria2, sets the following variables:
#   - LibAria2_FOUND
#   - LibAria2_INCLUDE_DIR (only if LibAria2_FOUND)
#   - LibAria2_LIBRARIES (only if LibAria2_FOUND)
include(FindPackageHandleStandardArgs)

find_path(LibAria2_INCLUDE_DIR NAMES aria2/aria2.h HINTS /usr/include)

find_library(LibAria2_LIBRARY NAMES libaria2 aria2 HINTS /usr/lib)

find_program(aria2c_EXECUTABLE aria2c)

if(aria2c_EXECUTABLE)
    execute_process(COMMAND ${aria2c_EXECUTABLE} --version OUTPUT_VARIABLE aria2c_VERSION_OUT)
    
    string(REGEX MATCH "aria2[ \t]version[ \t]([0-9.]+)" aria2_version_str ${aria2c_VERSION_OUT})
    string(REGEX MATCH "([0-9]+(\\.[0-9]+)+)" aria2_version ${aria2_version_str})

    string(REGEX MATCH "Libraries:([ \t][A-Za-z0-9/.-]+)+" aria2_libraries_str ${aria2c_VERSION_OUT})
    string(REGEX MATCHALL "[A-Za-z0-9/.-]+" aria2_libraries_w_version ${aria2_libraries_str})
    # take off the leading "Libraries" entry
    list(POP_FRONT aria2_libraries_w_version)
    foreach(libary_w_version IN LISTS aria2_libraries_w_version)
        string(REGEX REPLACE "/[0-9.]+" "" libary ${libary_w_version})
        string(TOLOWER ${libary} libary)
        list(APPEND aria2_libraries ${libary})
    endforeach()
    
endif()

find_package_handle_standard_args(
LibAria2
  REQUIRED_VARS
  LibAria2_LIBRARY
  LibAria2_INCLUDE_DIR
  VERSION_VAR aria2_version
)

if(LibAria2_FOUND)
    set(LibAria2_INCLUDE_DIR "${LIBUV_INCLUDE_DIR}")
    set(LibAria2_LIBRARIES ${aria2_libraries} ${LibAria2_LIBRARY})
    if(NOT TARGET Aria2::LibAria2)
        add_library(Aria2::LibAria2 UNKNOWN IMPORTED GLOBAL)
            set_target_properties(Aria2::LibAria2 PROPERTIES
            IMPORTED_LOCATION ${LibAria2_LIBRARY}
            INTERFACE_COMPILE_OPTIONS ${LibAria2_COMPILE_OPTIONS}
            INTERFACE_INCLUDE_DIRECTORIES ${LibAria2_INCLUDE_DIR}
            INTERFACE_LINK_LIBRARIES ${aria2_libraries}
        )
    endif()
endif()

# Hide internal variables
mark_as_advanced(LibAria2_LIBRARY LibAria2_INCLUDE_DIR LibAria2_LIBRARIES)

# Set standard variables
