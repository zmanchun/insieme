#
# Builds the insieme utils library
#
project(insieme_utils CXX)
message(STATUS "========== configure utils =============")
cmake_minimum_required(VERSION 3.2)

# configure config files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/insieme/utils/config.h)

# collect source files 
file(GLOB_RECURSE insieme_utils_srcs 	src/*.cpp )
insieme_glob_headers(insieme_utils_incs include)

# add include directory
#include_directories(include)

# build the utils library
add_library(insieme_utils ${LINKING_TYPE} ${insieme_utils_srcs} ${insieme_utils_incs} )
target_include_directories(insieme_utils PUBLIC include)

# lookup Boost library
insieme_find_package(NAME Boost COMPONENTS filesystem system regex) 
target_include_directories(insieme_utils SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
# add boost_filesystem library
target_link_libraries( insieme_utils ${Boost_FILESYSTEM_LIBRARY})
target_link_libraries( insieme_utils ${Boost_SYSTEM_LIBRARY})
# add boost_regex library
target_link_libraries(insieme_utils ${Boost_REGEX_LIBRARY})

# lookup LuaJIT library
insieme_find_package(NAME LUAJIT)
target_include_directories(insieme_utils PUBLIC ${LUAJIT_INCLUDE_DIRS})
target_link_libraries(insieme_utils ${LUAJIT_LIBRARIES})

target_link_libraries(insieme_utils dl)

cotire(insieme_utils)

# =============================================  TESTING  =====================================
set ( ut_prefix  ut_utils )
file(GLOB_RECURSE test_cases test/*.cc)
foreach ( case_file ${test_cases} )
	get_filename_component( case_name ${case_file} NAME_WE )

	insieme_fix_case_name( case_subdir ${case_file} )
	
	set ( case_name ${ut_prefix}${case_subdir}_${case_name} )
	add_executable(${case_name} ${case_file})
	target_link_libraries(${case_name} insieme_utils)
	target_link_libraries(${case_name} ${Boost_LIBRARIES})
	target_link_libraries(${case_name} ${Boost_PROGRAM_OPTIONS_LIBRARY})
	target_link_libraries(${case_name} ${Boost_FILESYSTEM_LIBRARY})
	target_link_libraries(${case_name} ${LUAJIT_LIBRARIES} )

	# exclude lua tests from valgrind since it fails for versions newer than 3.7.0
	set(VFLAG ${TRUE})
	if(case_file MATCHES ".*lua.*")
		set(VFLAG FALSE)
	endif(case_file MATCHES ".*lua.*")
	add_unit_test(${case_name} ${ut_prefix} ${VFLAG})
        
endforeach(case_file)
