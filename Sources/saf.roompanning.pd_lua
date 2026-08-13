local roompanning = pd.Class:new():register("saf.roompanning")

-- ╭─────────────────────────────────────╮
-- │            INITIALISATION           │
-- ╰─────────────────────────────────────╯
function roompanning:initialize(_, args)
	self.inlets = 1
	self.outlets = 2

	local rx = tonumber(args[1]) or 5
	local ry = tonumber(args[2]) or 5
	local rz = tonumber(args[3]) or 5
	local scale = tonumber(args[4]) or 40
	self.font_size = 10

	self.colors = {
		background1 = { 19, 47, 80 },
		background2 = { 27, 55, 87 },
		grid = { 46, 73, 102 },
		text = { 180, 195, 210 },
		source = { 255, 92, 92 },
		source_selected = { 255, 150, 150 },
		speaker = { 255, 255, 0 },
		receiver = { 0, 255, 169 },
	}

	self.room = {
		x = rx,
		y = ry,
		z = rz,
		prop = scale,
	}

	self.icon = {
		source = math.max(6, scale * 0.28),
		receiver = math.max(8, scale * 0.35),
		speaker = math.max(6, scale * 0.25),
	}

	self.view = "xy"
	self.sources = {}
	self.loudspeakers = {}
	self.dragging = nil

	self.receivers = {
		{
			coords = {
				x = rx * 0.5,
				y = ry * 0.5,
				z = rz * 0.5,
			},
			size = self.icon.receiver,
		},
	}

	self:rebuild_speakers(4)
	self:update_canvas_size()

	return true
end

-- ─────────────────────────────────────
function roompanning:postinitialize()
	self:outlet(1, "roomdim", { self.room.x, self.room.y, self.room.z })
	for i, recv in ipairs(self.receivers) do
		self:output_receiver(i, recv.coords)
	end
	for i, spk in ipairs(self.loudspeakers) do
		self:outlet(2, "speaker", { i, spk.azi, spk.ele })
	end
end

-- ╭─────────────────────────────────────╮
-- │              HELPERS                │
-- ╰─────────────────────────────────────╯
function roompanning:update_canvas_size()
	self.wsize, self.hsize = self:get_canvas_dimensions(self.view)
	self:set_size(self.wsize, self.hsize)
end

-- ─────────────────────────────────────
function roompanning:get_canvas_dimensions(view)
	local prop = self.room.prop
	if view == "xy" then
		return self.room.y * prop, self.room.x * prop
	elseif view == "yz" then
		return self.room.y * prop, self.room.z * prop
	elseif view == "xz" then
		return self.room.x * prop, self.room.z * prop
	end
	return self.room.y * prop, self.room.x * prop
end

-- ─────────────────────────────────────
function roompanning:clamp(value, minv, maxv)
	if value < minv then
		return minv
	elseif value > maxv then
		return maxv
	end
	return value
end

-- ─────────────────────────────────────
-- The room GUI uses the SAF listening convention (+X front, +Y left,
-- +Z up). The room simulator's public Y position, however, is measured from
-- the opposite side of the room and is flipped internally before generating
-- the spherical-harmonic directions. Mirror Y at this boundary so that a
-- source drawn on the left is encoded at a positive (leftward) azimuth.
function roompanning:to_roomsim_coords(coords)
	return coords.x, self.room.y - coords.y, coords.z
end

-- ─────────────────────────────────────
function roompanning:output_source(index, coords)
	local x, y, z = self:to_roomsim_coords(coords)
	self:outlet(1, "source", { index, x, y, z })
end

-- ─────────────────────────────────────
function roompanning:output_receiver(index, coords)
	local x, y, z = self:to_roomsim_coords(coords)
	self:outlet(1, "receiver", { index, x, y, z })
end

-- ─────────────────────────────────────
function roompanning:meters_to_pixels(xm, ym, zm, view)
	view = view or self.view
	local prop = self.room.prop
	if view == "xy" then
		local px = self.wsize - (ym * prop)
		local py = self.hsize - (xm * prop)
		return px, py
	elseif view == "yz" then
		local px = self.wsize - (ym * prop)
		local py = self.hsize - (zm * prop)
		return px, py
	elseif view == "xz" then
		local px = self.wsize - (xm * prop)
		local py = self.hsize - (zm * prop)
		return px, py
	end
	return 0, 0
end

-- ─────────────────────────────────────
function roompanning:pixels_to_meters(px, py, view, reference)
	view = view or self.view
	reference = reference or { x = 0, y = 0, z = 0 }
	local prop = self.room.prop

	local coords = {
		x = reference.x,
		y = reference.y,
		z = reference.z,
	}

	if view == "xy" then
		coords.y = (self.wsize - px) / prop
		coords.x = (self.hsize - py) / prop
	elseif view == "yz" then
		coords.y = (self.wsize - px) / prop
		coords.z = (self.hsize - py) / prop
	elseif view == "xz" then
		coords.x = (self.wsize - px) / prop
		coords.z = (self.hsize - py) / prop
	end

	coords.x = self:clamp(coords.x, 0, self.room.x)
	coords.y = self:clamp(coords.y, 0, self.room.y)
	coords.z = self:clamp(coords.z, 0, self.room.z)

	return coords
end

-- ─────────────────────────────────────
function roompanning:in_1_rename(args)
	local index = args[1]
	local name = args[2]
	self.sources[index].name = name
	self:repaint()
end

-- ─────────────────────────────────────
function roompanning:get_or_create_source(index)
	if not self.sources[index] then
		self.sources[index] = {
			coords = { x = 0, y = 0, z = 0 },
			selected = false,
			name = tostring(index),
		}
	end
	return self.sources[index]
end

-- ─────────────────────────────────────
function roompanning:rebuild_speakers(count)
	self.loudspeakers = {}
	count = math.max(0, math.floor(count or 0))
	if count == 0 then
		return
	end

	local radius = math.min(self.room.x, self.room.y) * 0.45
	local center = {
		x = self.room.x * 0.5,
		y = self.room.y * 0.5,
		z = self.room.z * 0.5,
	}

	for i = 0, count - 1 do
		local azi_deg = (360 / count) * i
		local azi_rad = math.rad(azi_deg)
		local x = center.x + radius * math.cos(azi_rad)
		local y = center.y + radius * math.sin(azi_rad)
		local z = center.z
		local ele_deg = 0

		self.loudspeakers[i + 1] = {
			azi = azi_deg,
			ele = ele_deg,
			coords = { x = x, y = y, z = z },
			size = self.icon.speaker,
		}
	end
end

-- ╭─────────────────────────────────────╮
-- │             INLETS                  │
-- ╰─────────────────────────────────────╯
function roompanning:in_1_numspeakers(args)
	local n = tonumber(args[1]) or 0
	self:rebuild_speakers(n)
	for i, spk in ipairs(self.loudspeakers) do
		self:outlet(2, "speaker", { i, spk.azi, spk.ele })
	end
	self:repaint()
end

-- ─────────────────────────────────────
function roompanning:in_1_source(args)
	local index = tonumber(args[1])
	if not index then
		return
	end

	local xm = tonumber(args[2]) or 0
	local ym = tonumber(args[3]) or 0
	local zm = tonumber(args[4]) or 0

	local src = self:get_or_create_source(index)
	src.coords = {
		x = self:clamp(xm, 0, self.room.x),
		y = self:clamp(ym, 0, self.room.y),
		z = self:clamp(zm, 0, self.room.z),
	}

	src.selected = false
	self:output_source(index, src.coords)
	self:repaint(2)
end

-- ─────────────────────────────────────
function roompanning:in_1_yzview(args)
	local flag = tonumber(args[1]) or 0
	self.view = flag == 1 and "yz" or "xy"
	self:update_canvas_size()
	self:repaint()
end

-- ─────────────────────────────────────
function roompanning:in_1_set(args)
	local cmd = args[1]
	if cmd == "source" then
		local index = tonumber(args[2])
		if not index then
			return
		end
		local xm = tonumber(args[3]) or 0
		local ym = tonumber(args[4]) or 0
		local zm = tonumber(args[5]) or 0

		local src = self:get_or_create_source(index)
		src.coords.x = self:clamp(xm, 0, self.room.x)
		src.coords.y = self:clamp(ym, 0, self.room.y)
		src.coords.z = self:clamp(zm, 0, self.room.z)
		src.selected = false

		self:output_source(index, src.coords)
		self:repaint(2)
	elseif cmd == "roomdim" then
		self.room.x = tonumber(args[2]) or self.room.x
		self.room.y = tonumber(args[3]) or self.room.y
		self.room.z = tonumber(args[4]) or self.room.z
		self.room.prop = tonumber(args[5]) or self.room.prop
		self.icon.source = math.max(6, self.room.prop * 0.28)
		self.icon.receiver = math.max(8, self.room.prop * 0.35)
		self.icon.speaker = math.max(6, self.room.prop * 0.25)
		self:update_canvas_size()
		self:repaint()
	end
end

-- ─────────────────────────────────────
function roompanning:in_1_reload()
	self:dofilex(self._scriptname)
	self:initialize("", {})
	self:repaint(1)
end

-- ─────────────────────────────────────
function roompanning:in_1_roomdim(args)
	local newX = tonumber(args[1]) or self.room.x
	local newY = tonumber(args[2]) or self.room.y
	local newZ = tonumber(args[3]) or self.room.z

	self.room.x = math.max(0.01, newX)
	self.room.y = math.max(0.01, newY)
	self.room.z = math.max(0.01, newZ)

	self:update_canvas_size()

	for _, src in pairs(self.sources) do
		src.coords.x = self:clamp(src.coords.x, 0, self.room.x)
		src.coords.y = self:clamp(src.coords.y, 0, self.room.y)
		src.coords.z = self:clamp(src.coords.z, 0, self.room.z)
	end

	for _, recv in ipairs(self.receivers) do
		recv.coords.x = self:clamp(recv.coords.x, 0, self.room.x)
		recv.coords.y = self:clamp(recv.coords.y, 0, self.room.y)
		recv.coords.z = self:clamp(recv.coords.z, 0, self.room.z)
	end

	self:outlet(1, "roomdim", { self.room.x, self.room.y, self.room.z })

	for idx, recv in ipairs(self.receivers) do
		self:output_receiver(idx, recv.coords)
	end
	for idx, src in pairs(self.sources) do
		self:output_source(idx, src.coords)
	end

	self:repaint()
end

function roompanning:in_1_receiver(args)
	local index = tonumber(args[1])
	if not index then
		return
	end

	local recv = self.receivers[index]
	if not recv then
		recv = {
			coords = { x = 0, y = 0, z = 0 },
			size = self.icon.receiver,
		}
		self.receivers[index] = recv
	end

	local xm = tonumber(args[2]) or recv.coords.x
	local ym = tonumber(args[3]) or recv.coords.y
	local zm = tonumber(args[4]) or recv.coords.z

	recv.coords.x = self:clamp(xm, 0, self.room.x)
	recv.coords.y = self:clamp(ym, 0, self.room.y)
	recv.coords.z = self:clamp(zm, 0, self.room.z)

	self:output_receiver(index, recv.coords)
	self:repaint(2)
end

-- ╭─────────────────────────────────────╮
-- │              MOUSE                  │
-- ╰─────────────────────────────────────╯
function roompanning:mouse_down(mx, my)
	local hit = false
	self.dragging = nil

	for idx, src in pairs(self.sources) do
		local px, py = self:meters_to_pixels(src.coords.x, src.coords.y, src.coords.z)
		local dx = mx - px
		local dy = my - py
		if dx * dx + dy * dy <= (self.icon.source * 0.5) ^ 2 then
			self.dragging = { type = "source", index = idx }
			src.selected = true
			hit = true 
		else
			src.selected = false
		end
	end

	if hit then
		self:repaint(2)
	end
end

-- ─────────────────────────────────────
function roompanning:mouse_drag(mx, my)
	if not self.dragging then
		return
	end

	local px = self:clamp(mx, 0, self.wsize)
	local py = self:clamp(my, 0, self.hsize)

	if self.dragging.type == "source" then
		local idx = self.dragging.index
		local src = self.sources[idx]
		if not src then
			return
		end

		local updated = self:pixels_to_meters(px, py, self.view, src.coords)
		src.coords.x = updated.x
		src.coords.y = updated.y
		src.coords.z = updated.z

		self:output_source(idx, updated)
		self:repaint(2)
	end
end

-- ─────────────────────────────────────
function roompanning:mouse_up()
	for _, src in pairs(self.sources) do
		src.selected = false
	end
	self.dragging = nil
	self:repaint(2)
end

-- ╭─────────────────────────────────────╮
-- │            DRAW HELPERS             │
-- ╰─────────────────────────────────────╯
function roompanning:draw_grid(g)
	local rows, cols
	local prop = self.room.prop

	if self.view == "xy" then
		rows = math.floor(self.room.x)
		cols = math.floor(self.room.y)
	elseif self.view == "yz" then
		rows = math.floor(self.room.z)
		cols = math.floor(self.room.y)
	else
		rows = math.floor(self.room.z)
		cols = math.floor(self.room.x)
	end

	g:set_color(table.unpack(self.colors.background1))
	g:fill_all()

	g:set_color(table.unpack(self.colors.background2))
	g:fill_rect(0, 0, self.wsize, self.hsize)

	g:set_color(table.unpack(self.colors.grid))
	for c = 0, cols do
		local x = self.wsize - (c * prop)
		g:stroke_rect(x, 0, 1, self.hsize, 1)
	end
	for r = 0, rows do
		local y = self.hsize - (r * prop)
		g:stroke_rect(0, y, self.wsize, 1, 1)
	end

	g:set_color(table.unpack(self.colors.text))
	if self.view == "xy" then
		g:draw_text("Left (+Y)", 4, self.hsize - 12, 120, self.font_size)
		g:draw_text("Front (+X)", self.wsize - 60, 4, 76, self.font_size)
	elseif self.view == "yz" then
		g:draw_text("Left (+Y)", 4, self.hsize - 12, 100, self.font_size)
		g:draw_text("Up (+Z)", self.wsize - 41, 4, 60, self.font_size)
	else
		error("View not supported")
	end
end

-- ─────────────────────────────────────
function roompanning:draw_speakers(g)
	g:set_color(table.unpack(self.colors.speaker))
	for _, spk in ipairs(self.loudspeakers) do
		local px, py = self:meters_to_pixels(spk.coords.x, spk.coords.y, spk.coords.z)
		local size = spk.size
		g:stroke_ellipse(px - size * 0.5, py - size * 0.5, size, size, 1)
	end
end

-- ─────────────────────────────────────
function roompanning:draw_sources(g)
	for index, src in pairs(self.sources) do
		local px, py = self:meters_to_pixels(src.coords.x, src.coords.y, src.coords.z)
		local size = self.icon.source
		local half = size * 0.5

		if src.selected then
			g:set_color(table.unpack(self.colors.source_selected))
			g:fill_ellipse(px - half, py - half, size, size)
		end

		g:set_color(table.unpack(self.colors.source))
		g:stroke_ellipse(px - half, py - half, size, size, 1)

		g:set_color(table.unpack(self.colors.text))
		g:draw_text(src.name, px + half + 3, py - half, #src.name * 10, 10)
	end
end

-- ─────────────────────────────────────
function roompanning:draw_receivers(g)
	g:set_color(table.unpack(self.colors.receiver))
	for _, recv in ipairs(self.receivers) do
		local px, py = self:meters_to_pixels(recv.coords.x, recv.coords.y, recv.coords.z)
		local size = recv.size
		local half = size * 0.5
		g:stroke_ellipse(px - half, py - half, size, size, 2)
	end
end

-- ╭─────────────────────────────────────╮
-- │                PAINT                │
-- ╰─────────────────────────────────────╯
function roompanning:paint(g)
	self:draw_grid(g)
	self:draw_speakers(g)
end

-- ─────────────────────────────────────
function roompanning:paint_layer_2(g)
	self:draw_sources(g)
	self:draw_receivers(g)
end
