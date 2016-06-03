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
	{ x=6,y=1,size=3 },
}

local c = canvas.new()
for i = 1, height+1 do
	c:line(grid,i*grid,grid * (width +1), i * grid)
end

for i = 1, width+1 do
	c:line(i*grid,grid,i*grid, grid*(height+1))
end

--[[
   0  1  2
    \ | /
  7 -   - 3
    / | \
   6  5  4
]]

local coord = {
	{ -1 , -1 },
	{ 0, -1 },
	{ 1, -1 },
	{ 1, 0 },
	{ 1, 1 },
	{ 0, 1 },
	{ -1, 1 },
	{ -1, 0 },
}

local function draw_block(c, x,y)
	local b = { pf.block(m, x, y) }
	for i,v in ipairs(b) do
		if v then
			local dx, dy = coord[i][1], coord[i][2]
			dx = dx * 5 - 2
			dy = dy * 5 - 2
			c:rect((x + 1) * grid + dx, (y+1) *grid + dy, 4, 4)
		end
	end
end

local function draw_wp(c, x, y)
	local xx = (x+1) * grid
	local yy = (y+1) * grid
	c:line(xx - 4,yy - 4, xx+4, yy+4)
	c:line(xx + 4,yy - 4, xx-4, yy+4)
end

for i = 0, 10 do
	for j = 0, 10 do
		draw_block(c,j,i)
	end
end

local path = { pf.path(m, 0,2, 6, 3) }

for i=1,#path,2 do
	draw_wp(c, path[i], path[i+1])
end

print(c:html())



