cmake_minimum_required(VERSION 2.8)
add_subdirectory(Box2D)

#notice the "recycling" of CMAKE_C_FLAGS
#this is necessary to pick up android flags
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic -fPIC" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -fPIC" )

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/API)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../shooter2)
set( LIBRARY_DEPS EGL GLESv2 log android)

set(MY_SRCS
    android_native_app_glue.c
    gl_code.cpp #cpp files can be compiled to
    )

set(CLANLIB_SRC
    ClanLib/Core/Math/vec1.cpp
    ClanLib/Core/Math/vec2.cpp
    ClanLib/Core/Math/vec3.cpp
    ClanLib/Core/Math/vec4.cpp
    ClanLib/Core/Math/mat2.cpp
    ClanLib/Core/Math/mat3.cpp
    ClanLib/Core/Math/mat4.cpp
    ClanLib/Core/Math/angle.cpp
    #ClanLib/Core/Text/string8.cpp
)
    
add_library(native-activity SHARED ${MY_SRCS} ${CLANLIB_SRC})

#link the module like any other
target_link_libraries(native-activity ${LIBRARY_DEPS} Box2D )
