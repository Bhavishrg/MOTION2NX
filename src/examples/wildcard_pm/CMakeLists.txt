add_executable(wildcard_pm wildcard_pm.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(wildcard_pm PRIVATE cxx_std_20)

target_link_libraries(wildcard_pm
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)
