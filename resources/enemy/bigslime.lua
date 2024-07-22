setSprite(1 + 7 * 2)
setHealth(4)
function act()
    while not isVisible() do
        yield()
    end
    while true do
        yield()
        yield()
        move(irandom(0, 3))
    end
end