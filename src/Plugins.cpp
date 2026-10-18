// Plugin entry point.
#include "common/FxBase.h"
#include "ducker/CatDucker.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CatDucker(); }
