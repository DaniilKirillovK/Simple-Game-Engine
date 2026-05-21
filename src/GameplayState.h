#pragma once
#include "IGameState.h"
#include "World.h"
#include <string>
#include "IRenderAdapter.h"

struct Mesh;

class GameplayState : public IGameState 
{
public:
    GameplayState(IRenderAdapter& renderer);

    virtual void onEnter() override;
    virtual void update(float deltaTime) override;
    virtual void render() override;

private:
    void setupEditorCallbacks();

    void enterPlayMode();
    void exitPlayMode();

    void setupTestScene();

    void saveSceneState();
    void restoreSceneState();

    void createEmptyEntity();
    void createCubeEntity();
    void createSphereEntity();
    void createFromAsset(Mesh* mesh, const std::string& path);

    std::string m_savedScenePath;

    bool m_playModeActive = false;

    IRenderAdapter& m_renderer;
    World* m_world;
};