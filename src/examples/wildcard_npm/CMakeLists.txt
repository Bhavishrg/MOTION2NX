add_executable(wildcard_npm wildcard_npm.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(wildcard_npm PRIVATE cxx_std_20)

target_link_libraries(wildcard_npm
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)
