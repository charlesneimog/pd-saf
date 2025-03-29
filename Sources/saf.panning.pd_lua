local panning = pd.Class:new():register("saf.panning")

-- ─────────────────────────────────────
function panning:initialize(name, args)
	self.inlets = 1
	self.outlets = 1
	if args == nil then
		args[1] = 1
	end
	self.size = 150
	self:set_size(self.size, self.size)
	self.repaint_sources = false
	self.selected = false

	-- Define colors with appropriate RGB values
	self.colors = {
		background1 = { 38, 48, 100}, -- obj color
		background2 = { 48, 58, 100}, -- obj inside circle
		lines = { 150, 150, 200 }, -- lines
		text = { 50, 50, 50 }, -- text
		sources = { 255, 0, 0 },
		source_text = { 0, 0, 0 },
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
	local distance = self.size / 3
	local angle_step = (math.pi * 2) / self.sources_size
	local angle = i * angle_step
	local x = center_x + math.cos(angle) * distance
	local y = center_y + math.sin(angle) * distance

	return {
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
	local distance = self.size / 3
	local angle_step = (math.pi * 2) / num_circles -- Espaçamento angular

	self.sources = {} -- Resetando a lista de círculos

	for i = 1, num_circles do
		local angle = i * angle_step -- Ângulo progressivo para cada círculo
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
			self:outlet(1, "list", { i, x, y })
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
	g:fill_ellipse(0, 0, size_x, size_y)

	-- Lines
	g:set_color(table.unpack(self.colors.lines))
	local center = size_x / 2
	g:draw_line(center, 0, center, self.size, 1)
	g:draw_line(0, center, self.size, center, 1)

	-- Ellipses
	local base_size = math.min(size_x, size_y) / 2
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
			local x = source.x - (source.size / 2)
			local y = source.y - (source.size / 2)
			local size = source.size
			g:set_color(table.unpack(source.color))
			g:stroke_ellipse(x, y, size, size, 1)

			local scale_factor = 0.7
			g:scale(scale_factor, scale_factor)
			local text_x, text_y = x - (size / 3), y - (size / 3)
			g:set_color(table.unpack(self.colors.source_text)) -- Use source_text color
			g:draw_text(tostring(i), text_x / scale_factor, text_y / scale_factor, 20, 3)
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
