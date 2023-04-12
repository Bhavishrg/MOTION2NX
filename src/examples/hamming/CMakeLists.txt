add_executable(hamming hamming.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(hamming PRIVATE cxx_std_20)

target_link_libraries(hamming
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)
