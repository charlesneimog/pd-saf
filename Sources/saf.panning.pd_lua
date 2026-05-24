local panning = pd.Class:new():register("saf.panning")

-- ─────────────────────────────────────
function panning:initialize(_, args)
	self.inlets = 1
	self.outlets = 2
	self.repaint_sources = false
	self.selected = false
	self.plan_size = 200
	self.fig_size = 200
	self.sources_size = 3
	self.margin = 5
	self.yzview = false
	self.nspeakers = 0

	pd.post("new saf")

	-- SAF / VST convention:
	-- +X = front
	-- +Y = left
	-- +Z = up
	--
	-- azimuth:
	--   0°   = front
	--   +90° = left
	--   -90° = right
	--   ±180 = back
	--
	-- elevation:
	--   +90° = up
	--   -90° = down

	self.colors = {
		background1 = { 19, 47, 80 },
		background2 = { 27, 55, 87 },
		speakers = { 255, 255, 0 },
		lines = { 46, 73, 102 },
		text = { 127, 145, 162 },
		sources = { 255, 0, 0 },
		source_text = { 230, 230, 240 },
	}

	for i, arg in ipairs(args) do
		if arg == "-size" then
			self.plan_size = type(args[i + 1]) == "number" and args[i + 1] or 200
			self.fig_size = self.plan_size
		elseif arg == "-nsources" then
			self.sources_size = type(args[i + 1]) == "number" and args[i + 1] or 5
		elseif arg == "-yzview" then
			local yz = type(args[i + 1]) == "number" and args[i + 1] or 0
			self.yzview = (yz == 1)
		elseif arg == "-nspeakers" then
			self.nspeakers = args[i + 1]
		end
	end

	if self.yzview then
		self.fig_size = self.plan_size * 2
	end

	self.speakers_pos = {}
	self.sources = {}

	self:set_size(self.fig_size, self.plan_size)

	for i = 1, self.sources_size do
		self.sources[i] = self:create_newsource(i)
	end

	return true
end

-- ─────────────────────────────────────
function panning:get_max_radius()
	return (self.plan_size / 2) - self.margin
end

-- ─────────────────────────────────────
-- SAF spherical coordinates:
--
-- x = front/back
-- y = left/right
-- z = up/down
--
-- screen:
-- right  = +screenX
-- down   = +screenY
--
-- therefore:
-- screenX = -y
-- screenY = -x
--
function panning:spherical_to_cartesian(azi_deg, ele_deg, radius)
	local center = self.plan_size / 2

	local azi = math.rad(azi_deg)
	local ele = math.rad(ele_deg)

	local x3d = math.cos(ele) * math.cos(azi) * radius
	local y3d = math.cos(ele) * math.sin(azi) * radius
	local z3d = math.sin(ele) * radius

	local screen_x = center - y3d
	local screen_y = center - x3d
	local screen_z = center - z3d

	return screen_x, screen_y, screen_z
end

-- ─────────────────────────────────────
function panning:cartesian_to_spherical(screen_x, screen_y)
	local center = self.plan_size / 2
	local max_radius = self:get_max_radius()

	-- inverse screen mapping
	local y3d = -(screen_x - center)
	local x3d = -(screen_y - center)

	local radius = math.sqrt(x3d * x3d + y3d * y3d)

	if radius > max_radius and radius > 0 then
		local scale = max_radius / radius
		x3d = x3d * scale
		y3d = y3d * scale
		radius = max_radius
	end

	local azi = math.atan(y3d, x3d)

	local azi_deg = math.deg(azi)

	return azi_deg, 0, radius
end

-- ─────────────────────────────────────
function panning:get_yz_center()
	local ellipse_x = self.plan_size + self.margin
	local ellipse_width = self.plan_size - (2 * self.margin)

	local center_x = ellipse_x + (ellipse_width / 2)
	local center_y = self.margin + (ellipse_width / 2)

	return center_x, center_y, ellipse_width / 2
end

-- ─────────────────────────────────────
-- YZ view:
--
-- horizontal = Y axis
-- vertical   = Z axis
--
-- SAF:
-- +Y = left
-- +Z = up
--
-- screen:
-- +X = right
-- +Y = down
--
-- therefore:
-- screenX = -Y
-- screenY = -Z
--
function panning:spherical_to_yz_screen(azi_deg, ele_deg, radius)
	local center_x, center_y, base_radius = self:get_yz_center()

	radius = math.min(radius or base_radius, base_radius)

	local azi = math.rad(azi_deg)
	local ele = math.rad(ele_deg)

	local y3d = math.cos(ele) * math.sin(azi) * radius
	local z3d = math.sin(ele) * radius

	local x = center_x - y3d
	local y = center_y - z3d

	return x, y
end

-- ─────────────────────────────────────
function panning:create_newsource(i)
	local max_radius = self:get_max_radius()

	local angle_step = (math.pi * 2) / self.sources_size
	local angle = (i - 1) * angle_step

	local azi_deg = math.deg(angle)
	local ele_deg = 0

	local radius = max_radius * 0.9

	local x, y, z = self:spherical_to_cartesian(azi_deg, ele_deg, radius)

	return {
		i = i,

		x = x,
		y = y,
		z = z,

		azi = azi_deg,
		ele = ele_deg,

		size = 8,

		color = self.colors.sources,

		fill = false,
		selected = false,

		radius = radius,
		dis = radius / max_radius,
	}
end

-- ─────────────────────────────────────
function panning:update_args()
	local args = {}

	table.insert(args, "-size")
	table.insert(args, self.plan_size)

	table.insert(args, "-nsources")
	table.insert(args, self.sources_size)

	if self.yzview then
		table.insert(args, "-yzview")
		table.insert(args, 1)
	end

	table.insert(args, "-nspeakers")
	table.insert(args, self.nspeakers)

	self:set_args(args)
end

--╭─────────────────────────────────────╮
--│ METHODS │
--╰─────────────────────────────────────╯

function panning:in_1_reload()
	self:dofilex(self._scriptname)
	self:initialize("", {})
	self:repaint()
end

-- ─────────────────────────────────────
function panning:in_1_yzview(args)
	local enable = args[1] == 1

	self.yzview = enable

	if enable then
		self.fig_size = self.plan_size * 2
		self:set_size(self.fig_size, self.plan_size)
	else
		self.fig_size = self.plan_size
		self:set_size(self.plan_size, self.plan_size)
	end

	self:update_args()
	self:repaint()
end

-- ─────────────────────────────────────
function panning:in_1_numspeakers(args)
	local n = args[1]

	self.nspeakers = n
	self.speakers_pos = {}

	for i = 0, self.nspeakers - 1 do
		local azi = (360 / self.nspeakers) * i

		self.speakers_pos[i + 1] = {
			azi = azi,
			ele = 0,
			dis = 1.0,
		}

		self:outlet(2, "speaker", { i + 1, azi, 0 })
	end

	self:repaint()
	self:update_args()
end

-- ─────────────────────────────────────
function panning:in_1_speaker(args)
	local n = args[1]

	self.speakers_pos[n] = self.speakers_pos[n] or {}

	self.speakers_pos[n].azi = args[2]
	self.speakers_pos[n].ele = args[3]
	self.speakers_pos[n].dis = args[4] or 1.0

	self:outlet(2, "speaker", { n, args[2], args[3] })

	self:repaint()
	self:update_args()
end

-- ─────────────────────────────────────
function panning:in_1_source(args)
	local index = args[1]

	local azi_deg = args[2]
	local ele_deg = args[3]

	local dis = args[4] or 0.8

	if index > self.sources_size then
		self.sources_size = index
		self:in_1_sources({ index })
	end

	self:outlet(1, "source", {
		index,
		azi_deg,
		ele_deg,
	})

	local max_radius = self:get_max_radius()

	local radius = max_radius * dis

	local x, y, z = self:spherical_to_cartesian(azi_deg, ele_deg, radius)

	local source = self.sources[index]

	source.x = x
	source.y = y
	source.z = z

	source.azi = azi_deg
	source.ele = ele_deg

	source.radius = radius
	source.dis = dis

	self:update_args()
	self:repaint(2)
end

-- ─────────────────────────────────────
function panning:in_1_size(args)
	local old_size = self.plan_size
	local relation = args[1] / old_size

	self.plan_size = args[1]

	local width = self.yzview and (self.plan_size * 2) or self.plan_size

	self.fig_size = width

	self:set_size(width, self.plan_size)

	local new_max_radius = self:get_max_radius()

	for _, source in pairs(self.sources) do
		source.radius = source.radius * relation
		source.dis = source.radius / new_max_radius

		source.x, source.y, source.z = self:spherical_to_cartesian(source.azi, source.ele, source.radius)
	end

	self:update_args()
	self:repaint()
end

-- ─────────────────────────────────────
function panning:in_1_sources(args)
	local num = args[1]

	self.sources_size = num
	self.sources = {}

	for i = 1, num do
		self.sources[i] = self:create_newsource(i)
	end

	self:outlet(1, "num_sources", { num })

	self:repaint(2)
end

--╭─────────────────────────────────────╮
--│ MOUSE │
--╰─────────────────────────────────────╯

function panning:mouse_down(x, y)
	local selected = false

	for i, source in pairs(self.sources) do
		local dx = x - source.x
		local dy = y - source.y

		local r = source.size / 2

		if (dx * dx + dy * dy) <= (r * r) then
			source.selected = not selected
			source.fill = true

			selected = true
		else
			source.selected = false
			source.fill = false
		end
	end

	self:repaint(2)
	self:repaint(3)
end

-- ─────────────────────────────────────
function panning:mouse_drag(x, y)
	local max_radius = self:get_max_radius()

	for i, source in pairs(self.sources) do
		if source.selected then
			local azi_deg, ele_deg, radius = self:cartesian_to_spherical(x, y)

			local sx, sy, sz = self:spherical_to_cartesian(azi_deg, ele_deg, radius)

			source.x = sx
			source.y = sy
			source.z = sz

			source.azi = azi_deg
			source.ele = ele_deg

			source.radius = radius
			source.dis = radius / max_radius

			self:outlet(1, "source", {
				i,
				azi_deg,
				ele_deg,
				source.dis,
			})
		end
	end

	self:repaint(2)
	self:repaint(3)
end

-- ─────────────────────────────────────
function panning:mouse_up(_, _)
	for _, source in pairs(self.sources) do
		source.selected = false
		source.fill = false
	end

	self:repaint(2)
	self:repaint(3)
end

--╭─────────────────────────────────────╮
--│ PAINT │
--╰─────────────────────────────────────╯

function panning:paint(g)
	local size = self.plan_size
	local center = size / 2
	local radius = self:get_max_radius()

	g:set_color(table.unpack(self.colors.background1))
	g:fill_all()

	g:set_color(table.unpack(self.colors.background2))
	g:fill_ellipse(self.margin, self.margin, size - (2 * self.margin), size - (2 * self.margin))

	g:set_color(table.unpack(self.colors.lines))

	g:draw_line(center, self.margin, center, size - self.margin, 1)

	g:draw_line(self.margin, center, size - self.margin, center, 1)

	for angle = 0, 2 * math.pi, math.pi / 8 do
		local x = center + math.cos(angle) * radius
		local y = center + math.sin(angle) * radius

		g:draw_line(center, center, x, y, 1)
	end

	for i = 1, 4 do
		local r = radius * (i / 4)

		g:stroke_ellipse(center - r, center - r, r * 2, r * 2, 1)
	end

	-- speakers
	for i = 1, self.nspeakers do
		local s = self.speakers_pos[i]

		if s then
			local x, y = self:spherical_to_cartesian(s.azi, s.ele, radius)

			g:set_color(table.unpack(self.colors.speakers))

			g:fill_ellipse(x - 3, y - 3, 6, 6)

			g:set_color(255, 255, 255)

			g:draw_text(tostring(i), x + 4, y + 4, 12, 3)
		end
	end

	g:set_color(table.unpack(self.colors.text))
	g:draw_text("xy view", 2, 2, 50, 1)

	-- YZ view
	if self.yzview then
		g:set_color(table.unpack(self.colors.background2))

		g:fill_ellipse(self.plan_size + self.margin, self.margin, size - (2 * self.margin), size - (2 * self.margin))

		g:set_color(table.unpack(self.colors.lines))

		g:draw_line(self.plan_size, 0, self.plan_size, self.plan_size, 1)

		g:set_color(table.unpack(self.colors.text))
		g:draw_text("yz view", self.plan_size + 2, 2, 50, 1)
	end
end

-- ─────────────────────────────────────
function panning:paint_layer_2(g)
	for i, source in pairs(self.sources) do
		if not source.selected then
			local x = source.x
			local y = source.y
			local size = source.size

			g:set_color(table.unpack(source.color))

			g:stroke_ellipse(x - (size / 2), y - (size / 2), size, size, 1)

			g:set_color(table.unpack(self.colors.source_text))

			g:draw_text(tostring(i), x + 4, y - 4, 12, 3)

			if self.yzview then
				local yz_x, yz_y = self:spherical_to_yz_screen(source.azi, source.ele, source.radius)

				g:set_color(table.unpack(source.color))

				g:stroke_ellipse(yz_x - (size / 2), yz_y - (size / 2), size, size, 1)

				g:set_color(table.unpack(self.colors.source_text))

				g:draw_text(tostring(i), yz_x + 4, yz_y - 4, 12, 3)
			end
		end
	end
end

-- ─────────────────────────────────────
function panning:paint_layer_3(g)
	for i, source in pairs(self.sources) do
		if source.selected then
			local size = source.size

			local x = source.x - (size / 2)
			local y = source.y - (size / 2)

			g:set_color(table.unpack(source.color))

			g:fill_ellipse(x, y, size, size)
			g:stroke_ellipse(x, y, size, size, 1)

			g:set_color(table.unpack(self.colors.source_text))

			g:draw_text(string.format("%d %.0f° %.0f°", i, source.azi, source.ele), x + 10, y + 10, 60, 1)

			if self.yzview then
				local yz_x, yz_y = self:spherical_to_yz_screen(source.azi, source.ele, source.radius)

				g:set_color(table.unpack(source.color))

				g:fill_ellipse(yz_x - (size / 2), yz_y - (size / 2), size, size)

				g:stroke_ellipse(yz_x - (size / 2), yz_y - (size / 2), size, size, 1)
			end
		end
	end
end
