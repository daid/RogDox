#ifndef MAIN_SCENE_H
#define MAIN_SCENE_H

#include <sp2/scene/scene.h>
#include <sp2/graphics/gui/widget/widget.h>

class Scene : public sp::Scene
{
public:
    Scene();
    ~Scene();

    virtual void onUpdate(float delta) override;

    sp::P<sp::gui::Widget> hud;
};

#endif//MAIN_SCENE_H
