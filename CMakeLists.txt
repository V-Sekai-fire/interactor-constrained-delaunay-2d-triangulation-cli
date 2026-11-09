cmake_minimum_required(VERSION 3.20)
project(tri2d_cli CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Standalone layout: this repo carries its own vendor/geogram-subset
# (no GarmentCode / curvenet-src directory assumptions).
set(GEOGRAM_SUBSET "${CMAKE_CURRENT_SOURCE_DIR}/vendor/geogram-subset")

file(GLOB GEOGRAM_SUBSET_SOURCES
  "${GEOGRAM_SUBSET}/geogram/basic/*.cpp"
  "${GEOGRAM_SUBSET}/geogram/numerics/*.cpp"
  "${GEOGRAM_SUBSET}/geogram/mesh/*.cpp"
  "${GEOGRAM_SUBSET}/geogram/delaunay/*.cpp"
  "${GEOGRAM_SUBSET}/geogram/points/*.cpp"
  "${GEOGRAM_SUBSET}/geogram/api/*.cpp"
  "${GEOGRAM_SUBSET}/geogram/bibliography/*.cpp"
  "${GEOGRAM_SUBSET}/geogram/third_party/predicate_generator/*.cpp"
  "${GEOGRAM_SUBSET}/geogram/third_party/numerics/*.cpp"
  "${GEOGRAM_SUBSET}/geogram/third_party/OpenNL/*.c"
)
add_library(geogram_subset STATIC ${GEOGRAM_SUBSET_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/boolean_expression.cpp")
target_include_directories(geogram_subset PUBLIC
  "${GEOGRAM_SUBSET}"
  "${GEOGRAM_SUBSET}/geogram/third_party"
  "${GEOGRAM_SUBSET}/zlib"
)
target_compile_definitions(geogram_subset PUBLIC _USE_MATH_DEFINES GEOGRAM_VERSION="1.9.9")
target_compile_options(geogram_subset PRIVATE -w)

add_executable(tri2d_cli tri2d_cli.cpp)
target_link_libraries(tri2d_cli PRIVATE geogram_subset)
