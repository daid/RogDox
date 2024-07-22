#pragma once

#include <sp2/graphics/gui/scene.h>


class IngameMenuScene : public sp::gui::Scene
{
public:
    IngameMenuScene();

    void onUpdate(float delta) override;

private:
    bool exit = false;
};
