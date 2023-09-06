add_executable(approx_pm approx_pm.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(approx_pm PRIVATE cxx_std_20)

target_link_libraries(approx_pm
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)