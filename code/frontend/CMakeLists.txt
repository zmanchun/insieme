#
# Builds the insieme frontend library
#
project(insieme_frontend CXX)

message(STATUS "========== configure frontend =============")
cmake_minimum_required(VERSION 3.2)

# collect source files
file(GLOB_RECURSE insieme_frontend_srcs	src/*.cpp src/*.def src/*.inc )
insieme_glob_headers(insieme_frontend_incs include)

# build the frontend library
add_library(insieme_frontend ${LINKING_TYPE} ${insieme_frontend_srcs} ${insieme_frontend_incs} )
target_include_directories(insieme_frontend PUBLIC ${insieme_frontend_include_dir})

# lookup Boost library
insieme_find_package(NAME Boost COMPONENTS serialization filesystem system program_options) #TODO NOT NEEDED? program_options system filesystem regex serialization 
target_include_directories(insieme_frontend SYSTEM PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(insieme_frontend ${Boost_SERIALIZATION_LIBRARY})
target_link_libraries(insieme_frontend ${Boost_FILESYSTEM_LIBRARY})
target_link_libraries(insieme_frontend ${Boost_SYSTEM_LIBRARY})
target_link_libraries(insieme_frontend ${Boost_PROGRAM_OPTIONS_LIBRARY})

# lookup LLVM/CLANG library+header
insieme_find_package(NAME LLVM)
target_link_libraries(insieme_frontend ${CLANG_LIBRARIES})
target_include_directories(insieme_frontend PUBLIC ${CLANG_INCLUDE_DIRS})
target_link_libraries(insieme_frontend ${LLVM_LIBRARIES})
target_include_directories(insieme_frontend PUBLIC ${LLVM_INCLUDE_DIRS})

# add dependencies
target_link_libraries(insieme_frontend insieme_core)
target_link_libraries(insieme_frontend insieme_utils)
target_link_libraries(insieme_frontend insieme_transform)
target_link_libraries(insieme_frontend insieme_annotations)

# required for llvm
add_definitions(-D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS)

# prevent clang Preprocessor.h from being included in cotire header to enable privacy violation workaround
set_target_properties(insieme_frontend PROPERTIES
	COTIRE_PREFIX_HEADER_IGNORE_PATH "${CMAKE_SOURCE_DIR};${CMAKE_BINARY_DIR};${CLANG_INCLUDE_DIRS}/clang/Lex/Preprocessor.h;${CLANG_INCLUDE_DIRS}/clang/Parse/Parser.h"
	COTIRE_PREFIX_HEADER_INCLUDE_PATH "/")
cotire(insieme_frontend)

# =============================================  TESTING  =====================================
# add custom target for ut_fe test suite
set ( ut_prefix  ut_fe )
add_custom_target(${ut_prefix})

file(GLOB_RECURSE test_cases test/*.cc)
foreach ( case_file ${test_cases})
	get_filename_component( case_name ${case_file} NAME_WE )
	
	insieme_fix_case_name( case_subdir ${case_file} )
	
	set ( case_name ${ut_prefix}${case_subdir}_${case_name} )
	add_executable(${case_name} ${case_file})

	#dependency on test suit target
	add_dependencies(${ut_prefix} ${case_name})

	target_link_libraries(${case_name} insieme_frontend)
	target_link_libraries(${case_name} insieme_core)
	target_link_libraries(${case_name} insieme_utils)
	#target_link_libraries(${case_name} insieme_driver)
	#target_link_libraries(${case_name} insieme_backend)
	#target_link_libraries(${case_name} insieme_transform)
	target_link_libraries(${case_name} insieme_annotations)
	target_link_libraries(${case_name} ${CLANG_LIBRARIES})
	target_link_libraries(${case_name} ${LLVM_LIBRARIES})
	
	if(${case_name} STREQUAL ut_fe_ocl_host_compiler_test)
		add_unit_test(${case_name} ${ut_prefix} off)
	else()
		add_unit_test(${case_name} ${ut_prefix}) 
	endif()
endforeach(case_file)
