local pf = require "pathfinding"
local canvas = require "canvas"

local height = 10
local width = 10
local grid = 30

local m = pf.new {
	width = width,
	height = height,
	{ x=1,y=1,size=2 },
	{ x=3,y=2,size=3 },
	{ x=7,y=1,size=3 },
	wall = {
		"......FFFF",
		"...IIIF...",
		"......A...",
		"......D...",
		"......E...",
		"......DCBA",
	}
}

local c = canvas.new()

for i = 0, height * 2 + 1 do
	for j = 0, width * 2 + 1 do
		local w = pf.block(m, j, i)
		if w > 0 then
			local a = string.format("%02X", 255 - w)
			c:rect(grid * j , grid * i, grid-1, grid-1, "#" .. a .. "FF" .. a)
		end
	end
end

local function draw_wp(c, x, y)
	local xx = (x+1) * grid
	local yy = (y+1) * grid
	c:line(xx - 4,yy - 4, xx+4, yy+4)
	c:line(xx + 4,yy - 4, xx-4, yy+4)
end

local path = { pf.path(m, 1, 4, 15, 9) }

for i=1,#path,2 do
	c:rect(path[i] * grid + 10, path[i+1] * grid + 10, 5, 5, "#0000ff")
end

print(c:html())



