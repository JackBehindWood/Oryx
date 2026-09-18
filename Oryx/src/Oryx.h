#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Log.h"
#include "Oryx/Core/Assert.h"
#include "Oryx/Core/Application.h"
#include "Oryx/Core/Layer.h"
#include "Oryx/Core/LayerStack.h"
#include "Oryx/Core/Random.h"
#include "Oryx/Core/Registry.h"

#include "Oryx/Events/Event.h"
#include "Oryx/Events/ApplicationEvent.h"

#include "Oryx/Math/Math.h"

#include "Oryx/Game/ActionId.h"
#include "Oryx/Game/Outcome.h"
#include "Oryx/Game/IState.h"
#include "Oryx/Game/IGame.h"
#include "Oryx/Game/Context.h"

#include "Oryx/Strategy/IStrategy.h"
#include "Oryx/Strategy/RandomStrategy.h"
#include "Oryx/Strategy/FirstLegalStrategy.h"
#include "Oryx/Strategy/MinimaxStrategy.h"
