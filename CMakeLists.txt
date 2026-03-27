cmake_minimum_required(VERSION 3.11)
project(GameEngine)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_CXX_STANDARD 17)

# --- GLFW ---
include(FetchContent)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.3.8)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# --- GLM ---
FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.1)
FetchContent_MakeAvailable(glm)

# --- ImGui ---
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.90.4)
FetchContent_MakeAvailable(imgui)
# ImGui 不是標準 CMake 專案，手動收集檔案
set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SOURCES 
    ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_widgets.cpp 
    ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# --- Assimp ---
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
# 強制關閉所有匯出功能與 3DS 相關組件
set(ASSIMP_BUILD_EXPORTERS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_3DS_IMPORTER OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_3DS_EXPORTER OFF CACHE BOOL "" FORCE)
# 禁用所有預設 Importers 以節省空間並修復 MinGW 編譯錯誤
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
# 僅啟用需要的格式
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)

FetchContent_Declare(assimp GIT_REPOSITORY https://github.com/assimp/assimp.git GIT_TAG v5.2.5)
FetchContent_MakeAvailable(assimp)

# --- Project ---
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")
add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE 
    include ${IMGUI_DIR} ${IMGUI_DIR}/backends
    ${assimp_SOURCE_DIR}/include
    ${assimp_BINARY_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    glfw glm::glm assimp::assimp opengl32 imm32
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()
