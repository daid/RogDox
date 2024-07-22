setSprite(0 + 7 * 2)
function act()
    while not isVisible() do
        yield()
    end
    while true do
        yield()
        yield()
        yield()
        move(irandom(0, 3))
    end
end