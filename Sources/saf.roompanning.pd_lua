local roompanning = pd.Class:new():register("saf.roompanning")

-- ─────────────────────────────────────
function roompanning:initialize(_, args)
	self.inlets = 1
	self.outlets = 2

	self.colors = {
		background1 = { 19, 47, 80 },
		background2 = { 27, 55, 87 },
		lines = { 46, 73, 102 },
		text = { 127, 145, 162 },
		sources = { 255, 0, 0 },
		source_text = { 230, 230, 240 },
	}

	self.roomsize = { x = 5, y = 5, z = 5, prop = 40 }
	self.loudspeakers = {
		{ x = 0.1 * self.roomsize.prop, y = 0.1 * self.roomsize.prop, z = 0, size = 20 },
		{ x = 0.1 * self.roomsize.prop, y = 4.5 * self.roomsize.prop, z = 0, size = 20 },
		{ x = 4.7 * self.roomsize.prop, y = 0.1 * self.roomsize.prop, z = 0, size = 20 },
		{ x = 4.7 * self.roomsize.prop, y = 4.5 * self.roomsize.prop, z = 0, size = 20 },
	}
	self.sources = {}
	self.receivers = {
		{
			x = (self.roomsize.x * self.roomsize.prop / 2) - 3,
			y = (self.roomsize.y * self.roomsize.prop / 2) - 3,
			size = 8,
			color = { 200, 200, 0 },
		},
	}

	self.wsize = self.roomsize.x * self.roomsize.prop
	self.hsize = self.roomsize.y * self.roomsize.prop

	self:set_size(self.wsize, self.hsize)

	return true
end

-- ──────────────────────────────────────────
function roompanning:update_args()
	-- local args = {
	-- 	"-plan_size",
	-- 	self.plan_size,
	-- }
	-- table.insert(args, "-sources_size")
	-- table.insert(args, self.sources_size)
	-- table.insert(args, "-fig_size")
	-- table.insert(args, self.fig_size)
	--
	-- if self.xzview == 1 then
	-- 	table.insert(args, "-xzview")
	-- 	table.insert(args, self.xzview)
	-- end
	--
	-- self:set_args(args)
end

--╭─────────────────────────────────────╮
--│               METHODS               │
--╰─────────────────────────────────────╯
function roompanning:in_1_reload()
	self:dofilex(self._scriptname)
	self:initialize("", {})
	self:repaint(1)
end

-- ─────────────────────────────────────
function roompanning:in_1_set(args)
	if #args < 1 then
		return
	end
	if args[1] == "loudspeakerpos" then
		self.loudspeakers[args[2]] = {
			x = args[3] * self.roomsize.prop,
			y = args[4] * self.roomsize.prop,
			size = args[5],
		}

		local speaker = self.loudspeakers[args[2]]
		local x_L = (self.roomsize.x * self.roomsize.prop) / 2
		local y_L = (self.roomsize.y * self.roomsize.prop) / 2
		local z_L = (self.roomsize.z * self.roomsize.prop) / 2
		local x_rel = speaker.x - x_L
		local y_rel = speaker.y - y_L
		local z_rel = (speaker.z or 0) - z_L -- Se não houver z definido, assuma 0

		local azi = math.deg(math.atan(y_rel, x_rel))
		local ele = math.deg(math.atan(z_rel, math.sqrt(x_rel ^ 2 + y_rel ^ 2)))
		self:outlet(2, "set", { "loudspeakerpos", args[2], azi, ele })
		self:repaint(2)
	elseif args[1] == "source" then
		local index = args[2]
		local x = args[3] * self.roomsize.prop
		local y = args[4] * self.roomsize.prop
		local z = args[5] * self.roomsize.prop
		self.sources[index] = {
			size = self.roomsize.prop / 6,
			x = x,
			y = y,
			z = z,
			fill = false,
			selected = false,
		}

		self:outlet(
			1,
			"set",
			{ "source", index, (x / self.wsize) * self.roomsize.x, (y / self.hsize) * self.roomsize.y }
		)

		self:repaint(3)
	elseif args[1] == "roomdim" then
		self.roomsize.x = args[2] -- x
		self.roomsize.y = args[3] -- y
		self.roomsize.z = args[4] -- z
		self.roomsize.prop = args[5] -- proportion meter:pixel
		self:set_size(self.roomsize.x * self.roomsize.prop, self.roomsize.y * self.roomsize.prop)

		self.wsize = self.roomsize.x * self.roomsize.prop
		self.hsize = self.roomsize.y * self.roomsize.prop
		self:repaint(1)
	end
end

--╭─────────────────────────────────────╮
--│                MOUSE                │
--╰─────────────────────────────────────╯
function roompanning:mouse_drag(x, y)
	local size_x, size_y = self:get_size()
	if x < 5 or x > (size_x - 5) or y < 5 or y > (size_y - 5) then
		return
	end

	for i, _ in pairs(self.sources) do
		if self.sources[i].selected then
			self.sources[i].x = x
			self.sources[i].y = y
			self.sources[i].fill = true
			self:outlet(
				1,
				"set",
				{ "source", i, (x / self.wsize) * self.roomsize.x, (y / self.hsize) * self.roomsize.y }
			)
		else
			self.sources[i].fill = false
		end
	end

	-- Repaint da interface gráfica
	self:repaint(3)
end

-- ─────────────────────────────────────
function roompanning:mouse_down(x, y)
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
function roompanning:mouse_up(_, _)
	for i, _ in pairs(self.sources) do
		self.sources[i].fill = false
		self.sources[i].selected = false
	end
	self:repaint(2)
	self:repaint(3)
end


-- ─────────────────────────────────────
function roompanning:properties()
    self:newframe("My Color Picker", 1)
    self:addcolorpicker("Background", "updatecolorbg");
end

-- ─────────────────────────────────────
function roompanning:updatecolorbg(arg)
    -- self.colors.background1
    --     		background1 = { 19, 47, 80 },

end


-- ─────────────────────────────────────
function roompanning:textinput1(args)
    for i=1, #args do
        pd.post(args[i])
    end
end

-- ─────────────────────────────────────
function roompanning:checkbox1(args)
    for i=1, #args do
        pd.post(args[i])
    end
end

--╭─────────────────────────────────────╮
--│                PAINT                │
--╰─────────────────────────────────────╯
function roompanning:draw_loudspeaker(g, x, y, size)
	-- Dimensões do corpo
	local body_w, body_h = size * 0.5, size
	-- Dimensões do alto-falante
	local speaker_w = size * 0.35

	-- Desenhar o corpo do alto-falante com cantos arredondados
	g:set_color(0, 0, 0) -- Preto
	g:fill_rect(x, y, body_w, body_h)

	-- Alto-falante maior (embaixo)
	g:set_color(255, 255, 255)
	g:stroke_ellipse(
		x + (body_w / 2) - (speaker_w / 2),
		y + (body_h / 2) - (speaker_w / 2) + speaker_w / 2,
		speaker_w,
		speaker_w,
		1
	)
	g:fill_ellipse(
		x + (body_w / 2) - (speaker_w / 2),
		y + (body_h / 2) - (speaker_w / 2) + speaker_w / 2,
		speaker_w,
		speaker_w
	)

	g:set_color(255, 255, 255)
	g:stroke_ellipse(x + (body_w / 2) - (speaker_w / 4), y + (body_h * 0.2), speaker_w / 2, speaker_w / 2, 1)
	g:fill_ellipse(x + (body_w / 2) - (speaker_w / 4), y + (body_h * 0.2), speaker_w / 2, speaker_w / 2)

	if true then
		return
	end
end

-- ─────────────────────────────────────
function roompanning:paint(g)
	g:set_color(table.unpack(self.colors.background1))
	g:fill_all()
	g:set_color(table.unpack(self.colors.lines))

	-- Define the number of small rectangles (rows and columns)
	local rows = 10
	local cols = 10

	-- Define padding and spacing between rectangles
	local rect_width = self.wsize / cols
	local rect_height = self.hsize / rows

	-- Draw the grid of small rectangles
	for row = 1, rows do
		for col = 1, cols do
			local x = 1 + (col - 1) * rect_width
			local y = 1 + (row - 1) * rect_height
			--g:stroke_rect(x, y, rect_width, rect_height, 1)
		end
	end

	g:set_color(table.unpack(self.colors.text))
end

-- ─────────────────────────────────────
function roompanning:paint_layer_2(g)
	-- draw loudspeakers
	for i = 1, #self.loudspeakers do
		self:draw_loudspeaker(g, self.loudspeakers[i].x, self.loudspeakers[i].y, self.loudspeakers[i].size)
	end
end

-- ─────────────────────────────────────
function roompanning:paint_layer_3(g)
	-- draw sources
	for i, v in pairs(self.sources) do
		g:set_color(255, 0, 0)
		local size = v.size
		g:stroke_ellipse(v.x, v.y, size, size, 1)
		if v.selected then
			g:fill_ellipse(v.x, v.y, size, size)
		end
		local scale_factor = 0.7
		g:scale(scale_factor, scale_factor)
		local text_x, text_y = v.x - (size / 3), v.y - (size / 3)
		g:set_color(255, 255, 255)
		g:draw_text(tostring(i), (text_x + 1) / scale_factor, (text_y - 1) / scale_factor, 20, 3)
		g:reset_transform()
	end

	-- draw receivers
	for i, v in pairs(self.receivers) do
		g:set_color(table.unpack(self.receivers[i].color))
		local size = v.size
		g:stroke_ellipse(v.x, v.y, size, size, 1)
	end
end

-- ─────────────────────────────────────
function roompanning:paint_layer_4(g)
	--
end
