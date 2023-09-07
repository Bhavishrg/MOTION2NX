add_executable(circuit_equality circuit_equality.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(circuit_equality PRIVATE cxx_std_20)

target_link_libraries(circuit_equality
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)
