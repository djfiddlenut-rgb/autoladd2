# Offline harness: drives the real AutoladdProcessor with synthetic voices and renders the editor to PNG.
#   cmake -B build-tests -DAUTOLADD_BUILD_TESTS=ON && cmake --build build-tests --target autoladd_harness
#   xvfb-run ./build-tests/Tests/autoladd_harness_artefacts/autoladd_harness <output-dir>
juce_add_console_app(autoladd_harness PRODUCT_NAME "autoladd_harness")

target_sources(autoladd_harness PRIVATE
    plugin_harness.cpp
    ../Source/PluginProcessor.cpp
    ../Source/PluginEditor.cpp)

target_include_directories(autoladd_harness PRIVATE ../Source)

target_compile_definitions(autoladd_harness PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_XRANDR=0
    JUCE_USE_XINERAMA=0
    JUCE_USE_XCURSOR=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(autoladd_harness PRIVATE
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_recommended_config_flags)
