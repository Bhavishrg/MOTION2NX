add_executable(naive_approx_pm naive_approx_pm.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(naive_approx_pm PRIVATE cxx_std_20)

target_link_libraries(naive_approx_pm
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)