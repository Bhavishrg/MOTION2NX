add_executable(test_dpf test_dpf.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(test_dpf PRIVATE cxx_std_20)

target_link_libraries(test_dpf
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)
