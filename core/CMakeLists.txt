add_library(robo_guide_core STATIC
    src/types.cpp
    src/verdict.cpp
    src/safety_gate.cpp
    src/test_definition.cpp
    src/util.cpp
)
target_include_directories(robo_guide_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(robo_guide_core PUBLIC cxx_std_17)
target_compile_definitions(robo_guide_core PUBLIC ROBO_GUIDE_ENABLE_MOTION=$<BOOL:${ROBO_GUIDE_ENABLE_MOTION}>)
set_target_properties(robo_guide_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(robo_guide_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ROBO_GUIDE_BUILD_TESTS)
    add_executable(robo_guide_core_tests tests/core_tests.cpp)
    target_link_libraries(robo_guide_core_tests PRIVATE robo_guide_core)
    add_test(NAME robo_guide_core_tests COMMAND robo_guide_core_tests)
endif()
