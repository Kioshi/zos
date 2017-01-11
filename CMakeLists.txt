cmake_minimum_required (VERSION 2.8)

set (PROJECT_NAME FATsym)
project (${PROJECT_NAME})   
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

if(CMAKE_COMPILER_IS_GNUCXX)
  add_compile_options(-std=c++11) # CMake 2.8.12 or newer
  set(CMAKE_CXX_STANDARD 11) # C++11...
  set(CMAKE_CXX_STANDARD_REQUIRED ON) #...is required...
  set(CMAKE_CXX_EXTENSIONS OFF) #...without compiler extensions like gnu++11
endif()

file(GLOB_RECURSE shared_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.h)

add_executable(${PROJECT_NAME} ${shared_SRCS})    
if(CMAKE_COMPILER_IS_GNUCXX)
  TARGET_LINK_LIBRARIES(${PROJECT_NAME}.exe pthread)                 
endif()
