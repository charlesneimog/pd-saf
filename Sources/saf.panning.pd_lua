local panning = pd.Class:new():register("saf.panning")

-- ─────────────────────────────────────
function panning:initialize(_, args)
	self.inlets = 1
	self.outlets = 1
	if args == nil then
		args[1] = 1
	end
	self.size = 200
	self:set_size(self.size, self.size)
	self.repaint_sources = false
	self.selected = false
	self.margin = 5

	-- Define colors with appropriate RGB values
	self.colors = {
		background1 = { 19, 47, 80 },
		background2 = { 27, 55, 87 }, -- obj inside circle
		lines = { 46, 73, 102 }, -- lines
		text = { 127, 145, 162 }, -- text
		sources = { 255, 0, 0 },
		source_text = { 230, 230, 240 },
	}

	self.sources = {}
	self.sources_size = args[1]
	if self.sources_size == nil then
		self.sources_size = 1
	end

	for i = 1, self.sources_size do
		self.sources[i] = self:create_newsource(i)
	end

	return true
end

-- ─────────────────────────────────────
function panning:create_newsource(i)
	local center_x, center_y = self:get_size() / 2, self:get_size() / 2
	local margin = self.margin -- Margin to keep the sources within the circle
	local max_radius = (self.size / 2) - margin
	local angle_step = (math.pi * 2) / self.sources_size
	local angle = (i - 1) * angle_step -- Correct the angle increment (starting at 0)
	local distance = max_radius * 0.9 -- Ensure sources are slightly inside the circle

	local x = center_x + math.cos(angle) * distance
	local y = center_y + math.sin(angle) * distance

	return {
		i = i,
		x = x,
		y = y,
		size = 8,
		color = self.colors.sources,
		fill = false,
		selected = false,
	}
end

--╭─────────────────────────────────────╮
--│               METHODS               │
--╰─────────────────────────────────────╯
function panning:in_1_reload()
	self:dofilex(self._scriptname)
	self:initialize("", {})
	self:repaint()
end

-- ─────────────────────────────────────
function panning:in_1_source(args)
	local index = args[1]
	local azi = math.rad(args[2])
	local ele = math.rad(args[3])
	if index > self.sources_size then
		self.sources_size = index
		self:in_1_sources({ index })
	end

	local x = math.cos(ele) * math.cos(azi + 90)
	local y = math.cos(ele) * math.sin(azi + 90)

	for _, source in pairs(self.sources) do
		if source.i == index then
			local adjusted_radius = (self.size / 2) - self.margin
			source.x = (x * adjusted_radius) + self.size / 2
			source.y = (y * adjusted_radius) + self.size / 2
		end
	end

	self:repaint(2)
end

-- ─────────────────────────────────────
function panning:in_1_size(args)
	local old_size = self.size
	self:set_size(args[1], args[1])
	self.size = args[1]
	local relation = self.size / old_size
	for _, source in pairs(self.sources) do
		source.x = source.x * relation
		source.y = source.y * relation
	end

	self:repaint(1)
	self:repaint(2)
	self:repaint(3)
end

-- ─────────────────────────────────────
function panning:in_1_sources(args)
	local num_circles = args[1]
	local center_x, center_y = self:get_size() / 2, self:get_size() / 2
	local angle_step = (math.pi * 2) / num_circles -- Espaçamento angular
	self.sources_size = args[1]

	self.sources = {}

	local margin = 10 -- Same margin used before
	local max_radius = (math.min(center_x, center_y) / 2) - margin
	local distance = max_radius * 0.9 -- Keep sources slightly inside the inner circle

	for i = 1, num_circles do
		local angle = i * angle_step -- Progressively increase the angle for each circle
		self.sources[i] = self:create_newsource(i)
		self.sources[i].x = center_x + math.cos(angle) * distance
		self.sources[i].y = center_y + math.sin(angle) * distance
	end

	self:repaint(2)
	self:outlet(1, "set", { "num_sources", args[1] })
end

--╭─────────────────────────────────────╮
--│                MOUSE                │
--╰─────────────────────────────────────╯
function panning:mouse_drag(x, y)
	local size_x, size_y = self:get_size()
	-- Verifica se o clique está dentro da área válida
	if x < 5 or x > (size_x - 5) or y < 5 or y > (size_y - 5) then
		return
	end

	for i, _ in pairs(self.sources) do
		if self.sources[i].selected then
			self.sources[i].x = x
			self.sources[i].y = y
			self.sources[i].fill = true

			-- TODO: Add z here
			local r = math.sqrt(x ^ 2 + y ^ 2)
			local azi = math.atan(y, x)
			local ele = math.atan(0, r)

			local azi_degrees = azi * (180 / math.pi)
			local ele_degrees = ele * (180 / math.pi)

			-- Output with azimuth and elevation in degrees
			self:outlet(1, "source", { i, azi_degrees, ele_degrees })
		else
			self.sources[i].fill = false
		end
	end

	-- Repaint da interface gráfica
	self:repaint(3)
end

-- ─────────────────────────────────────
function panning:mouse_down(x, y)
	local already_selected = false
	for i, source in pairs(self.sources) do
		local cx = source.x
		local cy = source.y
		local radius = source.size / 2
		local dx = x - cx
		local dy = y - cy
		if (dx * dx + dy * dy) <= (radius * radius) then
			self.sources[i].x = x
			self.sources[i].y = y
			self.sources[i].fill = true
			if not already_selected then
				self.sources[i].selected = true
				already_selected = true
			else
				self.sources[i].selected = false
			end
		else
			self.sources[i].fill = false
			self.sources[i].selected = false
		end
	end

	self:repaint(2)
	self:repaint(3)
end

-- ─────────────────────────────────────
function panning:mouse_up(_, _)
	for i, _ in pairs(self.sources) do
		self.sources[i].fill = false
		self.sources[i].selected = false
	end
	self:repaint(2)
	self:repaint(3)
end

--╭─────────────────────────────────────╮
--│                PAINT                │
--╰─────────────────────────────────────╯
function panning:paint(g)
	local size_x, size_y = self:get_size()
	if not self.colors then
		return
	end

	-- Use colors from self.colors
	g:set_color(table.unpack(self.colors.background1))
	g:fill_all()
	g:set_color(table.unpack(self.colors.background2))
	g:fill_ellipse(self.margin, self.margin, size_x - 2 * self.margin, size_y - 2 * self.margin)

	-- Lines
	g:set_color(table.unpack(self.colors.lines))
	local center = size_x / 2

	-- Adjusted vertical and horizontal lines
	g:draw_line(center, self.margin, center, size_y - self.margin, 1)
	g:draw_line(self.margin, center, size_x - self.margin, center, 1)

	-- Lines from center to border (radial lines)
	local base_radius = (math.min(size_x, size_y) / 2) - self.margin -- Radius of the circle
	for angle = 0, 2 * math.pi, math.pi / 8 do -- Change the angle increment for more/less lines
		local x_end = center + math.cos(angle) * base_radius
		local y_end = center + math.sin(angle) * base_radius
		g:draw_line(center, center, x_end, y_end, 1) -- Line from center to border
	end

	-- Ellipses
	local base_size = (math.min(size_x, size_y) / 2) - self.margin
	for i = 0, 3 do
		local scale = math.log(i + 1) / math.log(6)
		local radius_x = base_size * (1 - scale)
		local radius_y = base_size * (1 - scale)
		g:stroke_ellipse(center - radius_x, center - radius_y, radius_x * 2, radius_y * 2, 1)
	end

	-- Text
	local text_x, text_y = 1, 1
	g:set_color(table.unpack(self.colors.text))
	g:draw_text("xy view", text_x, text_y, 50, 1)

	self:repaint(2)
end

-- ─────────────────────────────────────
function panning:paint_layer_2(g)
	for i, source in pairs(self.sources) do
		if not self.sources[i].selected then
			-- Using the center of the circle, not the bottom-left corner
			local x = source.x
			local y = source.y
			local size = source.size

			-- Adjusting the drawing to make the ellipse centered at (x, y)
			g:set_color(table.unpack(source.color))
			g:stroke_ellipse(x - (size / 2), y - (size / 2), size, size, 1)

			local scale_factor = 0.7
			g:scale(scale_factor, scale_factor)

			-- Adjust the position of the text to be centered around the circle
			local text_x, text_y = x - (size / 3), y - (size / 3)
			g:set_color(table.unpack(self.colors.source_text))
			g:draw_text(tostring(i), text_x / scale_factor + 2, text_y / scale_factor + 2, 20, 3)

			g:reset_transform()
		end
	end
end

-- ─────────────────────────────────────
function panning:paint_layer_3(g)
	for i, source in pairs(self.sources) do
		if self.sources[i].selected then
			local x = source.x - (source.size / 2)
			local y = source.y - (source.size / 2)
			local size = source.size
			g:set_color(table.unpack(source.color))
			g:fill_ellipse(x, y, size, size)
			g:stroke_ellipse(x, y, size, size, 1)

			-- Source index text
			local text_x, text_y = x - (size / 1.5), y - (size / 2)
			g:set_color(table.unpack(self.colors.source_text)) -- Use source_text color
			g:draw_text(tostring(i), text_x, text_y, 10, 3)

			-- Coordinate text
			text_x, text_y = x + (size / 1), y + (size / 2)
			local scale_factor = 0.7
			g:scale(scale_factor, scale_factor)
			g:set_color(table.unpack(self.colors.source_text)) -- Use source_text color
			g:draw_text(
				tostring(math.floor(x)) .. " " .. tostring(math.floor(y)),
				text_x / scale_factor,
				text_y / scale_factor,
				40,
				1
			)
			g:reset_transform()
		end
	end
end
