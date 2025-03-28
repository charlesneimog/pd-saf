local panning = pd.Class:new():register("saf.panning")

-- ─────────────────────────────────────
function panning:initialize(name, args)
	self.inlets = 1
	self.outlets = 1
	if args == nil then
		args[1] = 1
	end
	self.size = 256
	self:set_size(self.size, self.size)
	self.repaint_sources = false
	self.selected = false

	self.sources = {}
	self.sources_size = 1
	for i = 1, self.sources_size do
		self.sources[i] = self:create_newsource()
	end

	return true
end

-- ─────────────────────────────────────
function panning:create_newsource()
	return {
		x = math.random(self.size),
		y = math.random(self.size),
		size = 8,
		color = { 217, 227, 127 },
		fill = false,
		selected = false,
	}
end
-- ─────────────────────────────────────
function panning:in_1_reload()
	self:dofilex(self._scriptname)
	self:initialize("", {})
	self:repaint()
end

-- ─────────────────────────────────────
function panning:in_1_size(args)
	self:set_size(args[1], args[1])
	self.size = args[1]
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

		local x = center_x + math.cos(angle) * distance
		local y = center_y + math.sin(angle) * distance

		self.sources[i] = {
			index = i,
			x = x,
			y = y,
			size = 10,
			color = { 200, 120, 120 },
			text = 1,
		}
	end

	self:repaint(2)
	self:outlet(1, "set", { "num_sources", args[1] })
end

-- ─────────────────────────────────────
function panning:mouse_drag(x, y)
	local size_x, size_y = self:get_size()
	-- Verifica se o clique está dentro da área válida
	if x < 5 or x > (size_x - 5) or y < 5 or y > (size_y - 5) then
		return
	end

	for i, source in pairs(self.sources) do
		local cx = source.x -- Centro do círculo X
		local cy = source.y -- Centro do círculo Y
		local radius = source.size / 2 -- Raio do círculo
		local dx = x - cx
		local dy = y - cy

		-- Verifica se o ponto está dentro do círculo
		if self.sources[i].selected then
			self.sources[i].x = x
			self.sources[i].y = y
			self.sources[i].fill = true
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
function panning:mouse_up(x, y)
	for i, _ in pairs(self.sources) do
		self.sources[i].fill = false
		self.sources[i].selected = false
	end
	self:repaint(2)
	self:repaint(3)
end

-- ─────────────────────────────────────
function panning:paint_layer_3(g)
	for i, source in pairs(self.sources) do
		if self.sources[i].selected then
			local x = source.x - (source.size / 2)
			local y = source.y - (source.size / 2)
			local size = source.size
			local color = source.color
			g:set_color(color[1], color[2], color[3])
			g:fill_ellipse(x, y, size, size)
			g:stroke_ellipse(x, y, size, size, 1)
			local text_x = x - (size / 1.5)
			local text_y = y - (size / 2)
			g:set_color(255, 255, 255)
			g:draw_text(tostring(i), text_x, text_y, 10, 3)
		end
	end
end

-- ─────────────────────────────────────
function panning:paint_layer_2(g)
	for i, source in pairs(self.sources) do
		if not self.sources[i].selected then
			local x = source.x - (source.size / 2)
			local y = source.y - (source.size / 2)
			local size = source.size
			local color = source.color
			g:set_color(color[1], color[2], color[3])
			g:stroke_ellipse(x, y, size, size, 1)
			local text_x = x - (size / 1.5)
			local text_y = y - (size / 2)
			g:set_color(255, 255, 255)
			g:draw_text(tostring(i), text_x, text_y, 20, 3)
		end
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
	g:draw_line(center, 0, center, self.size, 1)
	g:draw_line(0, center, self.size, center, 1)

	local base_size = math.min(size_x, size_y) / 2 -- Start with a base size
	for i = 0, 3 do
		local scale = math.log(i + 1) / math.log(6) -- Normalize log scale (adjust divisor for effect)
		local radius_x = base_size * (1 - scale)
		local radius_y = base_size * (1 - scale)

		g:stroke_ellipse(center - radius_x, center - radius_y, radius_x * 2, radius_y * 2, 1)
	end

	self:repaint(2)
end
