setSprite(0 + 7 * 4)
setHealth(4)
visible_counter = 0
function act()
    yield()
    if isVisible() then
        visible_counter = 3
    end
    if visible_counter > 0 then
        visible_counter = visible_counter - 1
        local d = pathFind()
        if d ~= nil then
            move(d)
        else
            move(irandom(0, 3))
        end
    end
end