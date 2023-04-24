add_executable(multni multni.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(multni PRIVATE cxx_std_20)

target_link_libraries(multni
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)
