#include "controller.h"


Controller::Controller()
: up("UP"), down("DOWN"), left("LEFT"), right("RIGHT")
{
    up.setKeys({"up", "keypad 8", "gamecontroller:0:button:dpup", "gamecontroller:0:axis:lefty"});
    down.setKeys({"down", "keypad 2", "gamecontroller:0:button:dpdown"});
    left.setKeys({"left", "keypad 4", "gamecontroller:0:button:dpleft"});
    right.setKeys({"right", "keypad 6", "gamecontroller:0:button:dpright", "gamecontroller:0:axis:leftx"});

    all.add(&up);
    all.add(&down);
    all.add(&left);
    all.add(&right);
}
