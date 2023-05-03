add_executable(exact_npm exact_npm.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(exact_npm PRIVATE cxx_std_20)

target_link_libraries(exact_npm
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)
