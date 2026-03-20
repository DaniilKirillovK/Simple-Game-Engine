#include "GameplayState.h"
#include "Logger.h"

void GameplayState::onEnter()
{
	LOG_INFO("Enter Gameplay State");
}

void GameplayState::update(float deltaTime, InputHandler& input)
{
}

void GameplayState::render(IRenderAdapter& renderer)
{
}
