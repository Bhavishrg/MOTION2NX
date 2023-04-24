add_executable(count count.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(count PRIVATE cxx_std_20)

target_link_libraries(count
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)
