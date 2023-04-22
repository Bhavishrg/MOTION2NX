add_executable(dot dot.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(dot PRIVATE cxx_std_20)

target_link_libraries(dot
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)
