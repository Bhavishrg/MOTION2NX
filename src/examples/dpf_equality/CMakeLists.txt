add_executable(dpf_equality dpf_equality.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(dpf_equality PRIVATE cxx_std_20)

target_link_libraries(dpf_equality
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)
