cmake_minimum_required(VERSION 3.23)
project(Project1 VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(JUCE)

juce_add_console_app(Project1 PRODUCT_NAME "CS120 Project1")

juce_generate_juce_header(Project1)

target_sources(Project1
        PRIVATE
        main.cpp
        MainComponent.cpp
        MainComponent.h
        )

target_compile_definitions(Project1
        PRIVATE
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Project1,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Project1,JUCE_VERSION>")

target_link_libraries(Project1
        PRIVATE
        juce::juce_analytics
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_box2d
        juce::juce_core
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_opengl
        juce::juce_osc
        juce::juce_product_unlocking
        juce::juce_video
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

find_package(Boost COMPONENTS system filesystem REQUIRED)
target_link_libraries(Project1 PRIVATE Boost::filesystem)
