cmake_minimum_required(VERSION 3.22)

project(ComposerMastermind VERSION 0.1.0 LANGUAGES C CXX)

# ------------------------------------------------------------
# C++ standard
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------
# JUCE
# ------------------------------------------------------------
# Expected layout:
#   ComposerMastermind/
#   ├── external/
#   │   └── JUCE/
#   ├── src/
#   └── CMakeLists.txt

add_subdirectory(external/JUCE)

# ------------------------------------------------------------
# Plugin target
# ------------------------------------------------------------
juce_add_plugin(ComposerMastermind
    COMPANY_NAME "YourCompany"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE

    PLUGIN_MANUFACTURER_CODE CMMD
    PLUGIN_CODE CPM1

    FORMATS VST3 Standalone
    PRODUCT_NAME "Composer Mastermind"
)

# ------------------------------------------------------------
# JUCE compatibility / automation fix
# ------------------------------------------------------------
target_compile_definitions(ComposerMastermind
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# ------------------------------------------------------------
# Source files
# ------------------------------------------------------------
target_sources(ComposerMastermind PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp

    src/ui/MainShellView.cpp
    src/ui/PrimaryView.cpp
    src/ui/PianoRollView.cpp
    src/ui/PatternSetupView.cpp
    src/ui/EditorView.cpp
    src/ui/InstanceView.cpp
    src/ui/SceneListComponent.cpp
    src/ui/SceneListView.cpp
    src/ui/DebugPanel.cpp
    src/ui/ArcGraphView.cpp
    src/ui/BlueprintArcCurveView.cpp
    src/ui/BlueprintSectionsContent.cpp
    src/ui/BlueprintSectionsView.cpp
    src/ui/BlueprintView.cpp
    src/ui/PresetLibraryContent.cpp
    src/ui/PresetLibraryView.cpp
    src/ui/GenerateView.cpp
    src/ui/ProposeView.cpp
    src/ui/ModulatorTargetView.cpp
    src/ui/PatternAwarenessView.cpp
    src/ui/ActivityLogView.cpp
    src/ui/MilestoneView.cpp

    src/composer/ComposerCore.cpp
    src/composer/PatternSyncServer.cpp
    src/composer/McpBridgeServer.cpp
    src/composer/TransportCompanionClient.cpp

    src/routing/InstanceRegistry.cpp
    src/routing/InstanceStateTracker.cpp
    src/routing/Router.cpp

    src/midi/CCDispatcher.cpp
    src/scheduling/Scheduler.cpp

    src/policy/SceneAdvancePolicy.cpp
    src/policy/MutationPolicy.cpp
    src/policy/PolicyEngine.cpp
    src/policy/Arc.cpp
    src/policy/ArcSet.cpp
    src/policy/CoherenceEvaluator.cpp
    src/policy/PresetResolver.cpp
    src/policy/BlueprintGenerator.cpp
    src/policy/MotifEngine.cpp
    src/policy/MonophonicOverlap.cpp
    src/policy/DurationCalculator.cpp
    src/policy/ArcShapeLibrary.cpp
    src/policy/NarrativeLaneSuggester.cpp
    src/policy/FactoryMotifPresets.cpp

    src/state/SceneLibrary.cpp
    src/state/BlueprintLibrary.cpp
    src/state/PresetLibrary.cpp
    src/state/ModulatorTargetLibrary.cpp
    src/state/ModulationRouteLibrary.cpp
    src/state/LockedStepLibrary.cpp
    src/state/MilestoneLibrary.cpp
    src/state/InstancePatternCache.cpp
    src/state/StateSerializer.cpp
    src/state/StateSnapshotStore.cpp
    src/state/CompositionBundleStore.cpp
    src/state/ProposalLibrary.cpp

    src/util/Validation.cpp
    src/util/JsonHelpers.cpp
)

# ------------------------------------------------------------
# Include directories
# ------------------------------------------------------------
target_include_directories(ComposerMastermind PRIVATE
    src
    src/plugin
    src/composer
    src/model
    src/policy
    src/scheduling
    src/routing
    src/midi
    src/state
    src/ui
    src/debug
    src/util
)

# ------------------------------------------------------------
# JUCE modules
# ------------------------------------------------------------
target_link_libraries(ComposerMastermind PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
)

# ------------------------------------------------------------
# Compiler warnings
# ------------------------------------------------------------
if (MSVC)
    target_compile_options(ComposerMastermind PRIVATE /W4)
else()
    target_compile_options(ComposerMastermind PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ------------------------------------------------------------
# Optional: group files nicely in IDEs
# ------------------------------------------------------------
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
    src/ui/MainShellView.cpp
    src/ui/PrimaryView.cpp
    src/ui/PianoRollView.cpp
    src/ui/PatternSetupView.cpp
    src/ui/EditorView.cpp
    src/ui/InstanceView.cpp
    src/ui/SceneListComponent.cpp
    src/ui/SceneListView.cpp
    src/ui/DebugPanel.cpp
    src/ui/ArcGraphView.cpp
    src/ui/BlueprintArcCurveView.cpp
    src/ui/BlueprintSectionsContent.cpp
    src/ui/BlueprintSectionsView.cpp
    src/ui/BlueprintView.cpp
    src/ui/PresetLibraryContent.cpp
    src/ui/PresetLibraryView.cpp
    src/ui/GenerateView.cpp
    src/ui/ProposeView.cpp
    src/ui/ModulatorTargetView.cpp
    src/ui/PatternAwarenessView.cpp
    src/ui/ActivityLogView.cpp
    src/ui/MilestoneView.cpp
    src/composer/ComposerCore.cpp
    src/composer/PatternSyncServer.cpp
    src/composer/McpBridgeServer.cpp
    src/composer/TransportCompanionClient.cpp
    src/routing/InstanceRegistry.cpp
    src/routing/InstanceStateTracker.cpp
    src/routing/Router.cpp
    src/midi/CCDispatcher.cpp
    src/midi/CCMapping.h    src/scheduling/Scheduler.cpp
    src/policy/SceneAdvancePolicy.cpp
    src/policy/MutationPolicy.cpp
    src/policy/PolicyEngine.cpp
    src/policy/Arc.cpp
    src/policy/ArcSet.cpp
    src/policy/CoherenceEvaluator.cpp
    src/policy/PresetResolver.cpp
    src/policy/BlueprintGenerator.cpp
    src/policy/MotifEngine.cpp
    src/policy/MonophonicOverlap.cpp
    src/policy/DurationCalculator.cpp
    src/policy/ArcShapeLibrary.cpp
    src/policy/NarrativeLaneSuggester.cpp
    src/policy/FactoryMotifPresets.cpp
    src/state/SceneLibrary.cpp
    src/state/BlueprintLibrary.cpp
    src/state/PresetLibrary.cpp
    src/state/ModulatorTargetLibrary.cpp
    src/state/ModulationRouteLibrary.cpp
    src/state/LockedStepLibrary.cpp
    src/state/MilestoneLibrary.cpp
    src/state/InstancePatternCache.cpp
    src/state/StateSerializer.cpp
    src/state/StateSnapshotStore.cpp
    src/state/CompositionBundleStore.cpp
    src/state/ProposalLibrary.cpp
    src/util/Validation.cpp
    src/util/JsonHelpers.cpp
)
