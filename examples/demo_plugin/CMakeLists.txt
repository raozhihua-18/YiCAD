add_library(YiCadDemoPlugin MODULE
    DemoPlugin.cpp)

if(WIN32)
    target_sources(YiCadDemoPlugin PRIVATE
        YiCadDemoPlugin.def)
endif()

target_link_libraries(YiCadDemoPlugin PRIVATE
    YiCAD::PluginSdk)

set_target_properties(YiCadDemoPlugin PROPERTIES
    FOLDER "Plugins"
    PREFIX ""
    OUTPUT_NAME "YiCadDemoPlugin")
