cmake_minimum_required(VERSION 3.23)

set(CURRENT_TARGET "Project1_Part3")

project(${CURRENT_TARGET} VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(JUCE)

juce_add_gui_app(${CURRENT_TARGET} PRODUCT_NAME ${CURRENT_TARGET})

juce_generate_juce_header(${CURRENT_TARGET})

if (${CURRENT_TARGET} STREQUAL "Project1_Part1_2")
    target_sources(${CURRENT_TARGET}
            PRIVATE
            part1&2/main.cpp
            part1&2/Part1&2.h
            )
elseif (${CURRENT_TARGET} STREQUAL "Project1_Part3")
    target_sources(${CURRENT_TARGET}
            PRIVATE
            part3/main.cpp
            part3/Part3.h
            part3/utils.h
            part3/utils.cpp
            )
elseif (${CURRENT_TARGET} STREQUAL "Project1_Part4")
    target_sources(${CURRENT_TARGET}
            PRIVATE
            part4/main.cpp
            part4/Part4.h
            part4/utils.h
            part4/utils.cpp
            )
endif ()


target_compile_definitions(${CURRENT_TARGET}
        PRIVATE
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        )

target_link_libraries(${CURRENT_TARGET}
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
target_link_libraries(${CURRENT_TARGET} PRIVATE Boost::filesystem)
