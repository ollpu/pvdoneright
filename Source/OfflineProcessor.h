#pragma once
#include "../JuceLibraryCode/JuceHeader.h"

#include "Common.h"

bool processWavFile(File inputPath, File outputPath, double scalingRatio, PlayerMode mode);
