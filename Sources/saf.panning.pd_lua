local panning = pd.Class:new():register("saf.panning")

-- ─────────────────────────────────────
function panning:initialize(name, args)
	self.inlets = 1
	self.outlets = 1
	if args == nil then
		args[1] = 1
	end
	self:set_size(128, 128)
	self.repaint_sources = false
	self.selected = false

	self.sources = {}
	self.sources_size = 1
	for i = 1, self.sources_size do
		self.sources[i] = { x = 127 / 2, y = 127 / 2, size = 8, color = { 207, 127, 127 }, fill = false }
	end

	return true
end

-- ─────────────────────────────────────
function panning:in_1_reload()
	self:dofilex(self._scriptname)
	self:initialize("", {})
	self:repaint()
end

-- ─────────────────────────────────────
function panning:mouse_drag(x, y)
	local size_x, size_y = self:get_size()
	if x < 5 or x > (size_x - 5) or y < 5 or y > (size_y - 5) then
		return
	end
	for i, _ in pairs(self.sources) do
		local cx = self.sources[i].x
		local cy = self.sources[i].y
		local radius = self.sources[1].size / 2
		if x >= cx - radius and x <= cx + radius and y >= cy - radius and y <= cy + radius then
			self.sources[i].x = x
			self.sources[i].y = y
			self:repaint(2)
		end
	end
end

-- ─────────────────────────────────────
function panning:mouse_down(x, y)
	for i, _ in pairs(self.sources) do
		local cx = self.sources[i].x
		local cy = self.sources[i].y
		local radius = self.sources[1].size / 2
		if x >= cx - radius and x <= cx + radius and y >= cy - radius and y <= cy + radius then
			self.sources[i].x = x
			self.sources[i].y = y
			self.sources[i].fill = true
			self:repaint(2)
		end
	end
end

-- ─────────────────────────────────────
function panning:mouse_up(x, y)
	for i, _ in pairs(self.sources) do
		self.sources[i].fill = false
	end
	self:repaint(2)
end

-- ─────────────────────────────────────
function panning:paint_layer_2(g)
	for _, source in pairs(self.sources) do
		local x = source.x - (source.size / 2)
		local y = source.y - (source.size / 2)
		local size = source.size
		local color = source.color
		g:set_color(color[1], color[2], color[3])
		if source.fill then
			g:fill_ellipse(x, y, size, size)
		end
		g:stroke_ellipse(x, y, size, size, 1)
	end
end

-- ─────────────────────────────────────
function panning:paint(g)
	local size_x, size_y = self:get_size()

	g:set_color(55, 85, 120)
	g:fill_all()
	g:set_color(80, 110, 150)
	g:fill_ellipse(0, 0, size_x, size_y)

	-- lines
	g:set_color(150, 150, 170)
	local center = size_x / 2
	g:draw_line(center, 0, center, 127, 1)
	g:draw_line(0, center, 127, center, 1)

	local base_size = math.min(size_x, size_y) / 2 -- Start with a base size
	for i = 0, 3 do
		local scale = math.log(i + 1) / math.log(6) -- Normalize log scale (adjust divisor for effect)
		local radius_x = base_size * (1 - scale)
		local radius_y = base_size * (1 - scale)

		g:stroke_ellipse(center - radius_x, center - radius_y, radius_x * 2, radius_y * 2, 1)
	end

	self:repaint(2)
end
