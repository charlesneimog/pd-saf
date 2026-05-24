local projection = pd.Class:new():register("saf.3droom2")

--╭─────────────────────────────────────╮
--│               BUTTON                │
--╰─────────────────────────────────────╯
local Button = {}
Button.__index = Button

function Button:new(father, x, y, w, h, font_size, label)
	local obj = {
		father = father,
		x = x,
		y = y,
		w = w,
		h = h,
		label = label,
		hovered = false,
		clicked = false,
		font_size = font_size,
	}

	setmetatable(obj, self)
	return obj
end

function Button:is_inside(mx, my)
	return mx >= self.x and mx <= self.x + self.w and my >= self.y and my <= self.y + self.h
end

function Button:on_mouse_click(callback)
	self.mouse_click_callback = callback
end

function Button:mouse_move(mx, my)
	local hovered = self:is_inside(mx, my)

	if hovered ~= self.hovered then
		self.hovered = hovered
		self.father:repaint(2)
	end
end

function Button:mouse_down(mx, my)
	if not self:is_inside(mx, my) then
		return false
	end

	self.clicked = true

	if self.mouse_click_callback then
		self.mouse_click_callback(self.father)
	end

	self.father:repaint(2)

	return true
end

function Button:mouse_up()
	self.clicked = false
	self.father:repaint(2)
end

function Button:draw(g)
	if self.clicked then
		g:set_color(180, 180, 180)
	elseif self.hovered then
		g:set_color(230, 230, 230)
	else
		g:set_color(200, 200, 200)
	end

	g:fill_rounded_rect(self.x, self.y, self.w, self.h, 2)

	g:set_color(0, 0, 0)
	g:stroke_rounded_rect(self.x, self.y, self.w, self.h, 2, 1)

	local text_w = #self.label * (self.font_size * 0.5)
	local text_h = self.font_size

	local tx = self.x + (self.w - text_w) * 0.5
	local ty = self.y + (self.h - text_h) * 0.5

	g:draw_text(self.label, tx, ty, text_w, text_h)
end

--╭─────────────────────────────────────╮
--│            INITIALIZE               │
--╰─────────────────────────────────────╯
function projection:initialize(name, args)
	self.inlets = 1
	self.outlets = 2

	self.coord_mode = "saf"
	self.origin_mode = "center"

	if type(args) == "table" then
		for i, arg in ipairs(args) do
			if arg == "-origin" then
				local mode = tostring(args[i + 1] or "")

				if mode == "center" or mode == "room" then
					self.origin_mode = mode
				end
			end
		end
	end

	self.rotation_x = 0
	self.rotation_y = 0

	self.last_mouse_x = 0
	self.last_mouse_y = 0

	self.animate = false
	self.now = 0

	self.clock_animation = pd.Clock:new():register(self, "point_animation")

	self.room_xyz = {
		x = 3,
		y = 3,
		z = 3,
	}

	self.room_xyz_internal = {
		x = 3,
		y = 3,
		z = 3,
	}

	self.scale_xyz = {
		x = 2,
		y = 2,
		z = 2,
	}

	self:update_internal_scale()

	self.points = {
		{ -0.5, -0.5, -0.5 },
		{ 0.5, -0.5, -0.5 },
		{ 0.5, 0.5, -0.5 },
		{ -0.5, 0.5, -0.5 },

		{ -0.5, -0.5, 0.5 },
		{ 0.5, -0.5, 0.5 },
		{ 0.5, 0.5, 0.5 },
		{ -0.5, 0.5, 0.5 },
	}

	self.speakers = {
		{ x = -0.5, y = 0, z = -0.5 },
		{ x = 0.5, y = 0, z = -0.5 },
		{ x = 0.5, y = 0, z = 0.5 },
		{ x = -0.5, y = 0, z = 0.5 },
	}

	self.trajectories = {}
	self:set_size(350, 350)

	local width = 350

	self.play_button = Button:new(self, width - 42, 3, 40, 20, 12, "play")
	self.reset_button = Button:new(self, width - 85, 3, 40, 20, 12, "reset")
	self.export_button = Button:new(self, width - 140, 3, 52, 20, 12, "export")

	self.play_button:on_mouse_click(self.play_click)
	self.reset_button:on_mouse_click(self.reset_click)
	self.export_button:on_mouse_click(self.export_click)

	return true
end

--╭─────────────────────────────────────╮
--│           TRAJECTORIES              │
--╰─────────────────────────────────────╯
function projection:get_trajectory(index)
	if not self.trajectories[index] then
		self.trajectories[index] = {
			x = {},
			y = {},
			z = {},
			time = {},
			point = nil,
			redraw = false,
		}
	end

	return self.trajectories[index]
end

-- ─────────────────────────────────────
function projection:is_valid_trajectory(t)
	if not t then
		return false
	end

	local x = t.x or {}
	local y = t.y or {}
	local z = t.z or {}

	return #x > 1 and #x == #y and #x == #z
end

-- ─────────────────────────────────────
function projection:count_trajectories()
	local count = 0

	for _, t in pairs(self.trajectories) do
		if self:is_valid_trajectory(t) then
			count = count + 1
		end
	end

	return count
end

--╭─────────────────────────────────────╮
--│              PLAYBACK               │
--╰─────────────────────────────────────╯
function projection:start_animation()
	self.animate = true
	self.now = 0

	for _, t in pairs(self.trajectories) do
		if self:is_valid_trajectory(t) then
			if #t.time ~= #t.x then
				t.time = {}
				for i = 1, #t.x do
					t.time[i] = (i - 1) * 2000
				end
			end

			t.point = {
				t.x[1],
				t.y[1],
				t.z[1],
			}

			t.redraw = true
		end
	end

	self.clock_animation:delay(0)
end

-- ─────────────────────────────────────
function projection:play_click()
	self:start_animation()
end

-- ─────────────────────────────────────
function projection:in_1_play()
	self:start_animation()
end

-- ─────────────────────────────────────
function projection:in_1_stop()
	self.animate = false
end

-- ─────────────────────────────────────
function projection:point_animation()
	if not self.animate then
		return
	end

	local all_finished = true

	for _, t in pairs(self.trajectories) do
		if self:is_valid_trajectory(t) then
			local total = t.time[#t.time]

			if self.now <= total then
				all_finished = false

				local segment = 1

				for i = 1, #t.time - 1 do
					if self.now >= t.time[i] and self.now <= t.time[i + 1] then
						segment = i
						break
					end
				end

				local t1 = t.time[segment]
				local t2 = t.time[segment + 1]
				local duration = math.max(1, t2 - t1)
				local alpha = (self.now - t1) / duration
				alpha = math.max(0, math.min(1, alpha))

				local function lerp(a, b, t)
					return a + (b - a) * t
				end

				t.point = {
					lerp(t.x[segment], t.x[segment + 1], alpha),
					lerp(t.y[segment], t.y[segment + 1], alpha),
					lerp(t.z[segment], t.z[segment + 1], alpha),
				}

				t.redraw = true
			end
		end
	end

	self:repaint(4)

	if all_finished then
		self.animate = false
		return
	end
	self.now = self.now + 30
	self.clock_animation:delay(30)
end

--╭─────────────────────────────────────╮
--│          COORDINATES                │
--╰─────────────────────────────────────╯
function projection:update_internal_scale()
	local rx = self.room_xyz.x
	local ry = self.room_xyz.y
	local rz = self.room_xyz.z

	if self.coord_mode == "saf" then
		self.room_xyz_internal = {
			x = ry,
			y = rz,
			z = rx,
		}
	else
		self.room_xyz_internal = {
			x = rx,
			y = ry,
			z = rz,
		}
	end

	local max_dim = math.max(self.room_xyz_internal.x, self.room_xyz_internal.y, self.room_xyz_internal.z)
	max_dim = math.max(1, max_dim)
	local s = 2 / max_dim

	self.scale_xyz = {
		x = self.room_xyz_internal.x * s,
		y = self.room_xyz_internal.y * s,
		z = self.room_xyz_internal.z * s,
	}
end

-- ─────────────────────────────────────
function projection:rotate_x(v, angle)
	local x, y, z = table.unpack(v)

	local c = math.cos(angle)
	local s = math.sin(angle)

	return {
		x,
		y * c - z * s,
		y * s + z * c,
	}
end

-- ─────────────────────────────────────
function projection:rotate_y(v, angle)
	local x, y, z = table.unpack(v)

	local c = math.cos(angle)
	local s = math.sin(angle)

	return {
		x * c + z * s,
		y,
		-x * s + z * c,
	}
end

-- ─────────────────────────────────────
function projection:project_point(v, scale, offsetX, offsetY, distance)
	local p = self:rotate_y(v, self.rotation_y)
	p = self:rotate_x(p, self.rotation_x)

	local depth = math.max(0.05, distance - p[3])

	local z = 1 / depth

	return {
		p[1] * z * scale + offsetX,
		p[2] * z * scale + offsetY,
		depth,
	}
end

--╭─────────────────────────────────────╮
--│               INPUTS                │
--╰─────────────────────────────────────╯
function projection:in_1_linex(args)
	local index = math.floor(args[1])
	local t = self:get_trajectory(index)
	t.x = {}
	for i = 2, #args do
		t.x[i - 1] = tonumber(args[i]) or 0
	end
	self:repaint(3)
end

-- ─────────────────────────────────────
function projection:in_1_liney(args)
	local index = math.floor(args[1])
	local t = self:get_trajectory(index)
	t.y = {}
	for i = 2, #args do
		t.y[i - 1] = tonumber(args[i]) or 0
	end
	self:repaint(3)
end

-- ─────────────────────────────────────
function projection:in_1_linez(args)
	local index = math.floor(args[1])
	local t = self:get_trajectory(index)

	t.z = {}
	for i = 2, #args do
		t.z[i - 1] = tonumber(args[i]) or 0
	end

	self:repaint(3)
end

-- ─────────────────────────────────────
function projection:in_1_time(args)
	local index = math.floor(args[1])
	local t = self:get_trajectory(index)
	t.time = {}
	for i = 2, #args do
		t.time[i - 1] = tonumber(args[i]) or 0
	end
end

-- ─────────────────────────────────────
function projection:in_1_roomdim(args)
	self.room_xyz = {
		x = tonumber(args[1]) or self.room_xyz.x,
		y = tonumber(args[2]) or self.room_xyz.y,
		z = tonumber(args[3]) or self.room_xyz.z,
	}

	self:update_internal_scale()
	self:repaint()
end

--╭─────────────────────────────────────╮
--│               BUTTONS               │
--╰─────────────────────────────────────╯
function projection:reset_click()
	self.rotation_x = 0
	self.rotation_y = 0

	self:repaint()
end

-- ─────────────────────────────────────
function projection:export_click()
	local file, err = io.open("trajectories.txt", "w")

	if not file then
		self:error(err)
		return
	end

	for index, t in pairs(self.trajectories) do
		if self:is_valid_trajectory(t) then
			file:write("trajectory ", index, "\n")
			for i = 1, #t.x do
				file:write(string.format("%.6f,%.6f,%.6f\n", t.x[i], t.y[i], t.z[i]))
			end
			file:write("\n")
		end
	end

	file:close()

	pd.post("export complete")
end

--╭─────────────────────────────────────╮
--│               MOUSE                 │
--╰─────────────────────────────────────╯
function projection:mouse_down(x, y)
	self.last_mouse_x = x
	self.last_mouse_y = y

	self.play_button:mouse_down(x, y)
	self.reset_button:mouse_down(x, y)
	self.export_button:mouse_down(x, y)
end

-- ─────────────────────────────────────
function projection:mouse_up(x, y)
	self.play_button:mouse_up()
	self.reset_button:mouse_up()
	self.export_button:mouse_up()
end

-- ─────────────────────────────────────
function projection:mouse_move(x, y)
	self.play_button:mouse_move(x, y)
	self.reset_button:mouse_move(x, y)
	self.export_button:mouse_move(x, y)
end

-- ─────────────────────────────────────
function projection:mouse_drag(x, y)
	local dx = x - self.last_mouse_x
	local dy = y - self.last_mouse_y

	self.rotation_y = self.rotation_y + dx * 0.01
	self.rotation_x = self.rotation_x - dy * 0.01

	self.last_mouse_x = x
	self.last_mouse_y = y

	self:repaint()
end

--╭─────────────────────────────────────╮
--│               COLORS                │
--╰─────────────────────────────────────╯
function projection:index_to_color(index)
	local colors = {
		{ 230, 25, 75 },
		{ 60, 180, 75 },
		{ 255, 225, 25 },
		{ 0, 130, 200 },
		{ 245, 130, 48 },
		{ 145, 30, 180 },
		{ 70, 240, 240 },
		{ 240, 50, 230 },
	}

	return colors[((index - 1) % #colors) + 1]
end

--╭─────────────────────────────────────╮
--│               PAINT                 │
--╰─────────────────────────────────────╯
function projection:paint(g)
	g:set_color(240, 240, 240)
	g:fill_all()

	local width, height = self:get_size()
	local max_scale = math.max(self.scale_xyz.x, self.scale_xyz.y, self.scale_xyz.z)
	local scale = (math.min(width, height) * 0.55) / max_scale

	local offsetX = width * 0.5
	local offsetY = height * 0.5

	local distance = 2
	local projected = {}

	for i, p in ipairs(self.points) do
		local scaled = {
			p[1] * self.scale_xyz.x,
			p[2] * self.scale_xyz.y,
			p[3] * self.scale_xyz.z,
		}
		projected[i] = self:project_point(scaled, scale, offsetX, offsetY, distance)
	end

	local edges = {
		{ 1, 2 },
		{ 2, 3 },
		{ 3, 4 },
		{ 4, 1 },
		{ 5, 6 },
		{ 6, 7 },
		{ 7, 8 },
		{ 8, 5 },
		{ 1, 5 },
		{ 2, 6 },
		{ 3, 7 },
		{ 4, 8 },
	}

	g:set_color(255, 255, 255)

	for _, edge in ipairs(edges) do
		local a = projected[edge[1]]
		local b = projected[edge[2]]

		g:draw_line(a[1], a[2], b[1], b[2], 1)
	end
end

-- ─────────────────────────────────────
function projection:paint_layer_2(g)
	self.play_button:draw(g)
	self.reset_button:draw(g)
	self.export_button:draw(g)
end

-- ─────────────────────────────────────
function projection:paint_layer_3(g)
	local width, height = self:get_size()
	local max_scale = math.max(self.scale_xyz.x, self.scale_xyz.y, self.scale_xyz.z)
	local scale = (math.min(width, height) * 0.5) / max_scale

	local offsetX = width * 0.5
	local offsetY = height * 0.5

	local distance = 2

	for index, t in pairs(self.trajectories) do
		if self:is_valid_trajectory(t) then
			local path = nil

			for i = 1, #t.x do
				local p = {
					t.x[i] * self.scale_xyz.x,
					t.y[i] * self.scale_xyz.y,
					t.z[i] * self.scale_xyz.z,
				}

				local proj = self:project_point(p, scale, offsetX, offsetY, distance)
				if not path then
					path = Path(proj[1], proj[2])
				else
					path:line_to(proj[1], proj[2])
				end
			end

			if path then
				g:set_color(table.unpack(self:index_to_color(index)))
				g:stroke_path(path, 1.5)
			end
		end
	end
end

-- ─────────────────────────────────────
function projection:paint_layer_4(g)
	local width, height = self:get_size()
	local max_scale = math.max(self.scale_xyz.x, self.scale_xyz.y, self.scale_xyz.z)
	local scale = (math.min(width, height) * 0.5) / max_scale

	local offsetX = width * 0.5
	local offsetY = height * 0.5

	local distance = 2

	for index, t in pairs(self.trajectories) do
		if t.point and t.redraw then
			local p = {
				t.point[1] * self.scale_xyz.x,
				t.point[2] * self.scale_xyz.y,
				t.point[3] * self.scale_xyz.z,
			}

			local proj = self:project_point(p, scale, offsetX, offsetY, distance)
			local depth = proj[3]
			local size = math.max(5, math.min(15, 20 / depth))
			local color = self:index_to_color(index)

			g:set_color(table.unpack(color))
			g:fill_ellipse(proj[1] - size * 0.5, proj[2] - size * 0.5, size, size)
			g:set_color(255, 255, 255)
			g:stroke_ellipse(proj[1] - size * 0.5, proj[2] - size * 0.5, size, size, 2)
		end
	end
end

--╭─────────────────────────────────────╮
--│               DEV                   │
--╰─────────────────────────────────────╯
function projection:in_1_reload()
	self:dofilex(self._scriptname)
	self:initialize("", {})
	self:repaint()
end
