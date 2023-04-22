add_executable(mult mult.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(mult PRIVATE cxx_std_20)

target_link_libraries(mult
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)
