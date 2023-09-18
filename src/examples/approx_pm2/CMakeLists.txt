add_executable(approx_pm2 approx_pm2.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(approx_pm2 PRIVATE cxx_std_20)

target_link_libraries(approx_pm2
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)