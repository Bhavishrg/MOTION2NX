add_executable(wildcard_pm wildcard_pmo.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(wildcard_pmo PRIVATE cxx_std_20)

target_link_libraries(wildcard_pmo
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)
