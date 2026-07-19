cmake_minimum_required(VERSION 3.28)
project(IPG_Lab2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The Koral SDK is located via CMAKE_PREFIX_PATH, which Koral Hub sets in the generated
# preset. The imported target is expected to propagate the public glm/imgui/spdlog usage
# requirements, so consumers don't find or link them explicitly.
find_package(Koral CONFIG REQUIRED)

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")
file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS "src/*.h" "src/*.hpp")

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE Koral::Koral)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/"
    SHADERS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")

# `cmake --build --preset <profile> --target run` builds the scene and launches it in the SDK
# runtime — the same thing Koral Hub's ▶ does, and what the IDE Run configurations invoke.
#
# KORAL_RUNTIME is set by the generated CMakePresets.json, which is machine-local. Keeping the
# path out of this file is what lets CMakeLists.txt stay committable and portable.
#
# The scene library is the only argument. Every run setting — API, window, and the directories the
# project keeps its assets and shaders in — lives in koral.json, which the runtime finds by walking
# up from the library it is handed. That is why this line does not have to know any of them, and
# why it cannot fall out of step with the Hub.
if(KORAL_RUNTIME)
    add_custom_target(run
        COMMAND "${KORAL_RUNTIME}" "$<TARGET_FILE:${PROJECT_NAME}>"
        DEPENDS ${PROJECT_NAME}
        USES_TERMINAL
        COMMENT "Running ${PROJECT_NAME} in the Koral runtime")
endif()
