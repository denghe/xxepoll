cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

########################################################################################

set(name xxepoll_test1)
set(SRCDIR test1)
project(${name})

include_directories(
	xxlib
	${SRCDIR}
)

set(SRCS)
file(GLOB SRCS	#GLOB_RECURSE
	xxlib/*.h
	${SRCDIR}/*.cpp
	${SRCDIR}/*.h
	${SRCDIR}/*.hpp
	${SRCDIR}/*.inc
)
add_executable(${name} ${SRCS})

target_precompile_headers(${name}
	PRIVATE	$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/${SRCDIR}/main.h>
)


########################################################################################

set(name xxepoll_test2)
set(SRCDIR test2)
project(${name})

include_directories(
		xxlib
		${SRCDIR}
)

set(SRCS)
file(GLOB SRCS	#GLOB_RECURSE
		xxlib/*.h
		${SRCDIR}/*.cpp
		${SRCDIR}/*.h
		${SRCDIR}/*.hpp
		${SRCDIR}/*.inc
		)
add_executable(${name} ${SRCS})

target_precompile_headers(${name}
		PRIVATE	$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/${SRCDIR}/main.h>
		)



########################################################################################

set(name xxepoll_test3)
set(SRCDIR test3)
project(${name})

include_directories(
		xxlib
		${SRCDIR}
)

set(SRCS)
file(GLOB SRCS	#GLOB_RECURSE
		xxlib/*.h
		${SRCDIR}/*.cpp
		${SRCDIR}/*.h
		${SRCDIR}/*.hpp
		${SRCDIR}/*.inc
		)
add_executable(${name} ${SRCS})

target_precompile_headers(${name}
		PRIVATE	$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/${SRCDIR}/main.h>
		)



########################################################################################

set(name xxepoll_test4)
set(SRCDIR test4)
project(${name})

include_directories(
		xxlib
		${SRCDIR}
)

set(SRCS)
file(GLOB SRCS	#GLOB_RECURSE
		xxlib/*.h
		${SRCDIR}/*.cpp
		${SRCDIR}/*.h
		${SRCDIR}/*.hpp
		${SRCDIR}/*.inc
		)
add_executable(${name} ${SRCS})

target_precompile_headers(${name}
		PRIVATE	$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/${SRCDIR}/main.h>
		)


########################################################################################

set(name xxepoll_test5)
set(SRCDIR test5)
project(${name})

include_directories(
		xxlib
		${SRCDIR}
)

set(SRCS)
file(GLOB SRCS	#GLOB_RECURSE
		xxlib/*.h
		${SRCDIR}/*.cpp
		${SRCDIR}/*.h
		${SRCDIR}/*.hpp
		${SRCDIR}/*.inc
		)
add_executable(${name} ${SRCS})

target_precompile_headers(${name}
		PRIVATE	$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/${SRCDIR}/main.h>
		)

########################################################################################

set(name xxepoll_test6)
set(SRCDIR test6)
project(${name})

include_directories(
		xxlib
		${SRCDIR}
)

set(SRCS)
file(GLOB SRCS	#GLOB_RECURSE
		xxlib/*.h
		${SRCDIR}/*.cpp
		${SRCDIR}/*.h
		${SRCDIR}/*.hpp
		${SRCDIR}/*.inc
		)
add_executable(${name} ${SRCS})

target_precompile_headers(${name}
		PRIVATE	$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/${SRCDIR}/main.h>
		)

########################################################################################

set(name xxepoll_test7)
set(SRCDIR test7)
project(${name})

include_directories(
		xxlib
		${SRCDIR}
)

set(SRCS)
file(GLOB SRCS	#GLOB_RECURSE
		xxlib/*.h
		${SRCDIR}/*.cpp
		${SRCDIR}/*.h
		${SRCDIR}/*.hpp
		${SRCDIR}/*.inc
		)
add_executable(${name} ${SRCS})

target_precompile_headers(${name}
		PRIVATE	$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/${SRCDIR}/main.h>
		)

########################################################################################

set(name xxepoll_test8)
set(SRCDIR test8)
project(${name})

include_directories(
		xxlib
		${SRCDIR}
)

set(SRCS)
file(GLOB SRCS	#GLOB_RECURSE
		xxlib/*.h
		${SRCDIR}/*.cpp
		${SRCDIR}/*.h
		${SRCDIR}/*.hpp
		${SRCDIR}/*.inc
		)
add_executable(${name} ${SRCS})

target_precompile_headers(${name}
		PRIVATE	$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/${SRCDIR}/main.h>
		)

########################################################################################

set(name xxepoll_test9)
set(SRCDIR test9)
project(${name})

include_directories(
		xxlib
		${SRCDIR}
)

set(SRCS)
file(GLOB SRCS	#GLOB_RECURSE
		xxlib/*.h
		${SRCDIR}/*.cpp
		${SRCDIR}/*.h
		${SRCDIR}/*.hpp
		${SRCDIR}/*.inc
		)
add_executable(${name} ${SRCS})

target_precompile_headers(${name}
		PRIVATE	$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/${SRCDIR}/main.h>
		)

########################################################################################

set(name xxepoll_test10)
set(SRCDIR test10)
project(${name})

include_directories(
		xxlib
		${SRCDIR}
)

set(SRCS)
file(GLOB SRCS	#GLOB_RECURSE
		xxlib/*.h
		${SRCDIR}/*.cpp
		${SRCDIR}/*.h
		${SRCDIR}/*.hpp
		${SRCDIR}/*.inc
		)
add_executable(${name} ${SRCS})

target_precompile_headers(${name}
		PRIVATE	$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/${SRCDIR}/main.h>
		)

########################################################################################

set(name xxepoll_test11)
set(SRCDIR test11)
project(${name})

include_directories(
		xxlib
		${SRCDIR}
)

set(SRCS)
file(GLOB SRCS	#GLOB_RECURSE
		xxlib/*.h
		${SRCDIR}/*.cpp
		${SRCDIR}/*.h
		${SRCDIR}/*.hpp
		${SRCDIR}/*.inc
		)
add_executable(${name} ${SRCS})

target_precompile_headers(${name}
		PRIVATE	$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/${SRCDIR}/main.h>
		)

########################################################################################

set(name xxepoll_test12)
set(SRCDIR test12)
project(${name})

include_directories(
		xxlib
		${SRCDIR}
)

set(SRCS)
file(GLOB SRCS	#GLOB_RECURSE
		xxlib/*.h
		${SRCDIR}/*.cpp
		${SRCDIR}/*.h
		${SRCDIR}/*.hpp
		${SRCDIR}/*.inc
		)
add_executable(${name} ${SRCS})

target_precompile_headers(${name}
		PRIVATE	$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/${SRCDIR}/main.h>
		)

########################################################################################

set(name xxepoll_test13)
set(SRCDIR test13)
project(${name})

include_directories(
		xxlib
		${SRCDIR}
)

set(SRCS)
file(GLOB SRCS	#GLOB_RECURSE
		xxlib/*.h
		${SRCDIR}/*.cpp
		${SRCDIR}/*.h
		${SRCDIR}/*.hpp
		${SRCDIR}/*.inc
		)
add_executable(${name} ${SRCS})

target_precompile_headers(${name}
		PRIVATE	$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/${SRCDIR}/main.h>
		)
