cmake_minimum_required(VERSION 3.22)
project(MADENCITUNE VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- DSP self-test (JUCE yok, CI'da eklentiden once calisir) ---
enable_testing()
add_executable(SelfTest Tests/SelfTest.cpp)
target_include_directories(SelfTest PRIVATE Source)
if(MSVC)
  target_compile_options(SelfTest PRIVATE /utf-8)
endif()
add_test(NAME SelfTest COMMAND SelfTest ${CMAKE_BINARY_DIR}/wav)   # onceki/sonraki WAV'lar build/wav/ altina yazilir

# --- JUCE (sabit surum, CI'da indirilir) ---
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.8
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# --- Gömülü varlıklar (font + görseller) -> BinaryData.h, adlar Source/Theme.h'da ---
juce_add_binary_data(MadenciAssets SOURCES
  Assets/SFPRODISPLAYBOLD.OTF
  Assets/knobicinkazma.png
  Assets/madenciyatayfoto.png
  Assets/madencidikeyfoto.jpg)

juce_add_plugin(MadenciTune
  COMPANY_NAME "Madenci"
  PLUGIN_MANUFACTURER_CODE Mdnc
  PLUGIN_CODE Mtun
  FORMATS VST3 Standalone
  PRODUCT_NAME "MADENCITUNE"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx)

target_sources(MadenciTune PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)
target_include_directories(MadenciTune PRIVATE Source)
if(MSVC)
  target_compile_options(MadenciTune PRIVATE /utf-8)   # Theme.h'daki Türkçe UTF-8 metinler için
endif()

target_compile_definitions(MadenciTune PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MadenciTune
  PRIVATE juce::juce_audio_utils MadenciAssets
  PUBLIC juce::juce_recommended_config_flags)
