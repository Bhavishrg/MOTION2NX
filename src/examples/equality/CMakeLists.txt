add_executable(equality equality.cpp)

find_package(Boost COMPONENTS json log program_options REQUIRED)

target_compile_features(equality PRIVATE cxx_std_20)

target_link_libraries(equality
    MOTION::motion
    Boost::json
    Boost::log
    Boost::program_options
)
