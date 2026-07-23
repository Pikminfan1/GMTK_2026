#pragma once

#include "Logging/LogMacros.h"

// Custom log categories so 10 people debugging different systems aren't all
// staring at one undifferentiated LogTemp. Filter by these in the Output Log.
DECLARE_LOG_CATEGORY_EXTERN(LogGMTKCore, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogGMTKCombat, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogGMTKAI, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogGMTKSpawn, Log, All);