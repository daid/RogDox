#include <sp2/engine.h>
#include <sp2/window.h>
#include <sp2/logging.h>
#include <sp2/io/resourceProvider.h>
#include <sp2/io/zipResourceProvider.h>
#include <sp2/io/fileSelectionDialog.h>
#include <sp2/audio/sound.h>
#include <sp2/audio/music.h>
#include <sp2/audio/musicPlayer.h>
#include <sp2/graphics/gui/scene.h>
#include <sp2/graphics/gui/theme.h>
#include <sp2/graphics/gui/loader.h>
#include <sp2/graphics/gui/widget/button.h>
#include <sp2/graphics/gui/widget/keynavigator.h>
#include <sp2/graphics/scene/graphicslayer.h>
#include <sp2/graphics/scene/basicnoderenderpass.h>
#include <sp2/graphics/scene/collisionrenderpass.h>
#include <sp2/graphics/textureManager.h>
#include <sp2/scene/scene.h>
#include <sp2/scene/node.h>
#include <sp2/scene/camera.h>
#include <sp2/io/keybinding.h>
#include <array>

#include "main.h"
#include "mainScene.h"
#include "ingameMenu.h"


sp::P<sp::Window> window;
sp::io::Keybinding escape_key("ESCAPE", {"Escape", "AC Back"});
Controller controller;


static void openOptionsMenu();
static void openCreditsMenu();
static void openControlsMenu();
void openMainMenu()
{
    sp::P<sp::gui::Widget> menu = sp::gui::Loader::load("gui/main_menu.gui", "MAIN_MENU");
    menu->getWidgetWithID("START")->setEventCallback([=](sp::Variant v) mutable {
        menu.destroy();
        new Scene();
    });
    menu->getWidgetWithID("OPTIONS")->setEventCallback([=](sp::Variant v) mutable {
        menu.destroy();
        openOptionsMenu();
    });
    menu->getWidgetWithID("CREDITS")->setEventCallback([=](sp::Variant v) mutable {
        menu.destroy();
        openCreditsMenu();
    });
    menu->getWidgetWithID("QUIT")->setEventCallback([](sp::Variant v){
        sp::Engine::getInstance()->shutdown();
    });
#ifdef EMSCRIPTEN
    menu->getWidgetWithID("QUIT")->hide();
#endif
}

static void openOptionsMenu()
{
    sp::P<sp::gui::Widget> menu = sp::gui::Loader::load("gui/main_menu.gui", "OPTIONS_MENU");
    menu->getWidgetWithID("EFFECT_VOLUME")->getWidgetWithID("VALUE")->setAttribute("caption", sp::string(int(sp::audio::Sound::getVolume())) + "%");
    menu->getWidgetWithID("EFFECT_VOLUME")->getWidgetWithID("SLIDER")->setAttribute("value", sp::string(sp::audio::Sound::getVolume()));
    menu->getWidgetWithID("EFFECT_VOLUME")->getWidgetWithID("SLIDER")->setEventCallback([=](sp::Variant v) mutable {
        menu->getWidgetWithID("EFFECT_VOLUME")->getWidgetWithID("VALUE")->setAttribute("caption", sp::string(v.getInteger()) + "%");
        sp::audio::Sound::setVolume(v.getInteger());
    });
    menu->getWidgetWithID("MUSIC_VOLUME")->getWidgetWithID("VALUE")->setAttribute("caption", sp::string(int(sp::audio::Music::getVolume())) + "%");
    menu->getWidgetWithID("MUSIC_VOLUME")->getWidgetWithID("SLIDER")->setAttribute("value", sp::string(sp::audio::Music::getVolume()));
    menu->getWidgetWithID("MUSIC_VOLUME")->getWidgetWithID("SLIDER")->setEventCallback([=](sp::Variant v) mutable {
        menu->getWidgetWithID("MUSIC_VOLUME")->getWidgetWithID("VALUE")->setAttribute("caption", sp::string(v.getInteger()) + "%");
        sp::audio::Music::setVolume(v.getInteger());
    });
    menu->getWidgetWithID("CONTROLS")->setEventCallback([=](sp::Variant v) mutable {
        menu.destroy();
        openControlsMenu();
    });
    menu->getWidgetWithID("BACK")->setEventCallback([=](sp::Variant v) mutable {
        menu.destroy();
        openMainMenu();
    });
}

class Rebinder : public sp::Node
{
public:
    Rebinder(sp::P<sp::gui::Widget> parent, sp::P<sp::io::Keybinding> binding, sp::io::Keybinding::Type bind_type)
    : sp::Node(parent), binding(binding), bind_type(bind_type)
    {
        binding->startUserRebind(bind_type);
        sp::P<sp::gui::Widget> w = getParent();
        parent->setAttribute("caption", "?");
        findNavigator(getScene()->getRoot())->disable();
    }

    virtual void onUpdate(float delta) override
    {
        if (!binding->isUserRebinding())
        {
            //TODO: Remove oldest bind of there are too many keys of this type.
            int count = 0;
            for(int n=0; binding->getKeyType(n) != sp::io::Keybinding::Type::None; n++)
                if (binding->getKeyType(n) & bind_type)
                    count += 1;
            if (count > 2)
            {
                for(int n=0; binding->getKeyType(n) != sp::io::Keybinding::Type::None; n++)
                {
                    if (binding->getKeyType(n) & bind_type)
                    {
                        binding->removeKey(n);
                        break;
                    }
            }
            }
            sp::P<sp::gui::Widget> w = getParent();
            for(int n=0; binding->getKeyType(n) != sp::io::Keybinding::Type::None; n++)
                w->setAttribute("caption", binding->getHumanReadableKeyName(n));
            findNavigator(getScene()->getRoot())->enable();
            delete this;
        }
    }

private:
    sp::P<sp::gui::KeyNavigator> findNavigator(sp::P<sp::Node> node)
    {
        if (sp::P<sp::gui::KeyNavigator>(node))
            return node;
        for(auto child : node->getChildren())
        {
            auto result = findNavigator(child);
            if (result)
                return result;
        }
        return nullptr;
    }

    sp::P<sp::io::Keybinding> binding;
    sp::io::Keybinding::Type bind_type;
};

static void openControlsMenu()
{
    sp::gui::Loader loader("gui/main_menu.gui");
    sp::P<sp::gui::Widget> menu = loader.create("CONTROLS_MENU");
    //new Rebinder(menu);

    std::array<sp::io::Keybinding::Type, 4> key_types{
        sp::io::Keybinding::Type::Keyboard,
        sp::io::Keybinding::Type::Keyboard,
        sp::io::Keybinding::Type::Controller,
        sp::io::Keybinding::Type::Controller,
    };
    for(auto keybinding : controller.all)
    {
        sp::P<sp::gui::Widget> keybinding_menu = loader.create("@CONTROLS_KEYBINDING", menu->getWidgetWithID("KEYS"));
        keybinding_menu->getWidgetWithID("NAME")->setAttribute("caption", keybinding->getLabel() + ":");
        int done = 0;
        for(int n=0; n<4; n++)
        {
            auto button = loader.create("@CONTROLS_KEYBINDING_BUTTON", keybinding_menu);
            button->setAttribute("caption", "");
            for(int m=0; keybinding->getKeyType(m) != sp::io::Keybinding::Type::None; m++)
            {
                if (!(done & (1 << m)) && (keybinding->getKeyType(m) & key_types[n]))
                {
                    done |= 1 << m;
                    button->setAttribute("caption", keybinding->getHumanReadableKeyName(m));
                    break;
                }
            }
            button->setEventCallback([=](sp::Variant v) mutable
            {
                new Rebinder(button, keybinding, key_types[n]);
            });
        }
    }
    menu->getWidgetWithID("BACK")->setEventCallback([=](sp::Variant v) mutable {
        menu.destroy();
        openOptionsMenu();
    });
}

class AutoCreditScroller : public sp::Node
{
public:
    using sp::Node::Node;

    virtual void onUpdate(float delta) override
    {
        sp::P<sp::gui::Widget> w = getParent();
        w->layout.position.y -= delta * 30.0f;
        if (w->layout.position.y < -w->layout.size.y)
            w->layout.position.y = 480.0f;
    }
};

static void openCreditsMenu()
{
    sp::P<sp::gui::Widget> menu = sp::gui::Loader::load("gui/main_menu.gui", "CREDITS_MENU");
    new AutoCreditScroller(menu->getWidgetWithID("CREDITS"));
    menu->getWidgetWithID("BACK")->setEventCallback([=](sp::Variant v) mutable {
        menu.destroy();
        openMainMenu();
    });
}

int main(int argc, char** argv)
{
    sp::P<sp::Engine> engine = new sp::Engine();

    //Create resource providers, so we can load things.
    sp::io::ResourceProvider::createDefault();

    //Disable or enable smooth filtering by default, enabling it gives nice smooth looks, but disabling it gives a more pixel art look.
    sp::texture_manager.setDefaultSmoothFiltering(false);

    //Create a window to render on, and our engine.
    window = new sp::Window();
#if !defined(DEBUG) && !defined(EMSCRIPTEN)
    window->setFullScreen(true);
#endif

    sp::gui::Theme::loadTheme("default", "gui/theme/basic.theme.txt");
    new sp::gui::Scene(sp::Vector2d(640, 480));

    sp::P<sp::SceneGraphicsLayer> scene_layer = new sp::SceneGraphicsLayer(1);
    scene_layer->addRenderPass(new sp::BasicNodeRenderPass());
#ifdef DEBUG
    scene_layer->addRenderPass(new sp::CollisionRenderPass());
#endif
    window->addLayer(scene_layer);

    sp::audio::Music::setVolume(50);
    new sp::audio::MusicPlayer("music");
    new IngameMenuScene();
    openMainMenu();

    engine->run();

    return 0;
}
