add_executable(approximate_pm approximate_pm.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(approximate_pm PRIVATE cxx_std_20)

target_link_libraries(approximate_pm
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)
