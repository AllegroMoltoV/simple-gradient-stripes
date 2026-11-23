cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.c src/gradient-stripes-source.cpp)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# 描画テスト。buildspec.json の dependencies.obs-studio.version と dependencies.prebuilt.version にそろえる。
# .deps の下のフォルダー名に版が入るため
set(RENDER_TEST_OBS_VERSION "31.1.1" CACHE STRING "OBS Studio version in buildspec.json")
set(RENDER_TEST_DEPS_VERSION "2025-07-11" CACHE STRING "Prebuilt obs-deps version in buildspec.json")

# tests/render-test.cpp が受け付けるシナリオの名前。render-test.yaml の SCENARIOS と同じにする
set(
  RENDER_TEST_SCENARIOS
  horizontal
  vertical
  rotation
  period
  softness
  transparent-background
  still
  flow
  custom-size
)

# Windows では常にビルドする。macOS と Linux では CI の render-test.yaml が有効にする
option(ENABLE_RENDER_TESTS "Build the render test on macOS and Linux" OFF)
if(WIN32 OR ENABLE_RENDER_TESTS)
  add_executable(render-test tests/render-test.cpp)
  target_compile_features(render-test PRIVATE cxx_std_17)
  target_link_libraries(render-test PRIVATE OBS::libobs)
  add_dependencies(render-test ${CMAKE_PROJECT_NAME})
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)
    target_link_libraries(render-test PRIVATE X11::X11)
  endif()
endif()

if(WIN32)
  enable_testing()

  # 各シナリオを Direct3D 11 と OpenGL の両方で登録する。グラフィックスのモジュールは OBS のビルドから先に作る:
  # cmake --build .deps/obs-studio-<version>/build_x64 --config Release --target libobs-d3d11 libobs-opengl
  # CI は libobs-d3d11 だけを作り、d3d11 のテストだけを実行する (ctest -R d3d11)
  set(_obs_source "${CMAKE_CURRENT_SOURCE_DIR}/.deps/obs-studio-${RENDER_TEST_OBS_VERSION}")
  set(_graphics_d3d11 "${_obs_source}/build_x64/libobs-d3d11/Release/libobs-d3d11.dll")
  set(_graphics_opengl "${_obs_source}/build_x64/libobs-opengl/Release/libobs-opengl.dll")
  foreach(_backend IN ITEMS d3d11 opengl)
    foreach(_scenario IN LISTS RENDER_TEST_SCENARIOS)
      set(_test_name render-${_backend}-${_scenario})
      add_test(
        NAME ${_test_name}
        COMMAND
          render-test "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>" "${CMAKE_CURRENT_SOURCE_DIR}/data"
          "${_graphics_${_backend}}" "${_obs_source}/libobs/data" ${_scenario}
      )
      # libobs とその依存は .deps から読み込む
      set_tests_properties(
        ${_test_name}
        PROPERTIES
          ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${CMAKE_CURRENT_SOURCE_DIR}/.deps/bin/64bit;PATH=path_list_prepend:${CMAKE_CURRENT_SOURCE_DIR}/.deps/obs-deps-${RENDER_TEST_DEPS_VERSION}-x64/bin"
      )
    endforeach()
  endforeach()
endif()
