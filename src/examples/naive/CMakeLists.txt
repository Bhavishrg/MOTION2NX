add_executable(naive naive.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(naive PRIVATE cxx_std_20)

target_link_libraries(naive
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)