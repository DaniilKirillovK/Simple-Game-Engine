#pragma once
#include "IGameState.h"
#include "World.h"
#include <string>
#include "IRenderAdapter.h"

class GameplayState : public IGameState 
{
public:
    GameplayState(IRenderAdapter& renderer);

    virtual void onEnter() override;
    virtual void update(float deltaTime) override;
    virtual void render() override;

private:
    void enterPlayMode();
    void exitPlayMode();

    void setupTestScene();

    void saveSceneState();
    void restoreSceneState();

    std::string m_savedScenePath;

    bool m_playModeActive = false;

    IRenderAdapter& m_renderer;
    World* m_world;
};