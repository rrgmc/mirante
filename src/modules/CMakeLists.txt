# Adds every module folder that has a CMakeLists.txt, in alphabetical order.
# See docs/modules.md#building.
file(GLOB module_dirs LIST_DIRECTORIES true CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*")
list(SORT module_dirs)
foreach(dir IN LISTS module_dirs)
    if(IS_DIRECTORY "${dir}" AND EXISTS "${dir}/CMakeLists.txt")
        add_subdirectory("${dir}")
    endif()
endforeach()
