#include "ingameMenu.h"
#include "main.h"

#include <sp2/engine.h>
#include <sp2/graphics/gui/loader.h>


IngameMenuScene::IngameMenuScene()
: sp::gui::Scene(sp::Vector2d(640, 480), "INGAME_MENU", 101)
{
    disable();
}

void IngameMenuScene::onUpdate(float delta)
{
    if (escape_key.getDown())
    {
        if (getRootWidget()->getChildren().empty())
        {
            auto gui = sp::gui::Loader::load("gui/ingame_menu.gui", "INGAME_MENU", getRootWidget());
            gui->getWidgetWithID("EXIT")->setEventCallback([this](sp::Variant)
            {
                exit = true;
            });

            gui->getWidgetWithID("BACK")->setEventCallback([this](sp::Variant)
            {
                for(auto node : getRootWidget()->getChildren())
                    node.destroy();
                sp::Engine::getInstance()->setGameSpeed(1.0);
            });
            sp::Engine::getInstance()->setGameSpeed(0.0);
        }
        else
        {
            for(auto node : getRootWidget()->getChildren())
                node.destroy();
            sp::Engine::getInstance()->setGameSpeed(1.0);
        }
    }

    if (exit)
    {
        for(auto node : getRootWidget()->getChildren())
            node.destroy();
        sp::Engine::getInstance()->setGameSpeed(1.0);
        sp::Scene::get("MAIN").destroy();
        openMainMenu();
        exit = false;
    }

    sp::gui::Scene::onUpdate(delta);
}
