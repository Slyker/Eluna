cmake_minimum_required(VERSION 3.14)
project(ElunaTests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(GTest REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUA REQUIRED lua5.2)

# Test stub include path (overrides game-server headers)
set(STUB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/stubs)

# Source root for Eluna
set(ELUNA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# ============================================================
# Base64 test (ElunaUtility encode/decode)
# ============================================================
# We compile a stripped version of ElunaUtility.cpp that only
# contains the Base64 functions (no game-server deps).
add_executable(test_base64
    test_base64.cpp
    base64_impl.cpp
)
target_include_directories(test_base64 PRIVATE
    ${STUB_INCLUDE_DIR}
)
target_link_libraries(test_base64 GTest::gtest GTest::gtest_main)

# ============================================================
# LuaVal test (pure C++ + Lua interaction)
# ============================================================
add_executable(test_luaval
    test_luaval.cpp
    ${ELUNA_ROOT}/LuaValue.cpp
    ${ELUNA_ROOT}/ElunaCompat.cpp
)
target_include_directories(test_luaval PRIVATE
    ${STUB_INCLUDE_DIR}
    ${ELUNA_ROOT}
    ${LUA_INCLUDE_DIRS}
)
target_compile_options(test_luaval PRIVATE
    "-include${STUB_INCLUDE_DIR}/ElunaUtility.h"
)
target_link_libraries(test_luaval GTest::gtest GTest::gtest_main ${LUA_LIBRARIES})

# ============================================================
# BindingMap key types and hash_helper test
# ============================================================
add_executable(test_bindingmap
    test_bindingmap.cpp
)
target_include_directories(test_bindingmap PRIVATE
    ${STUB_INCLUDE_DIR}
    ${ELUNA_ROOT}
    ${LUA_INCLUDE_DIRS}
)
target_compile_options(test_bindingmap PRIVATE
    "-include${STUB_INCLUDE_DIR}/ElunaUtility.h"
)
target_link_libraries(test_bindingmap GTest::gtest GTest::gtest_main ${LUA_LIBRARIES})

# ============================================================
# LuaEvent state management test
# ============================================================
add_executable(test_luaevent
    test_luaevent.cpp
)
target_include_directories(test_luaevent PRIVATE
    ${STUB_INCLUDE_DIR}
    ${ELUNA_ROOT}
)
target_link_libraries(test_luaevent GTest::gtest GTest::gtest_main)

# ============================================================
# ElunaConfig logic test (ShouldMapLoadEluna, TokenizeAllowedMaps)
# ============================================================
add_executable(test_elunaconfig
    test_elunaconfig.cpp
)
target_include_directories(test_elunaconfig PRIVATE
    ${STUB_INCLUDE_DIR}
    ${ELUNA_ROOT}
)
target_link_libraries(test_elunaconfig GTest::gtest GTest::gtest_main)

enable_testing()
add_test(NAME Base64Tests COMMAND test_base64)
add_test(NAME LuaValTests COMMAND test_luaval)
add_test(NAME BindingMapTests COMMAND test_bindingmap)
add_test(NAME LuaEventTests COMMAND test_luaevent)
add_test(NAME ElunaConfigTests COMMAND test_elunaconfig)
