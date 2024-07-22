setSprite(1 + 7 * 7)
setHealth(15)
setCanBreakWalls(true)
function act()
    while not isVisible() do
        yield()
    end
    while true do
        yield()
        yield()
        local d = pathFind()
        if d ~= nil then
            move(d)
        else
            move(irandom(0, 3))
        end
    end
end