local mcmeter = pd.Class:new():register("mc.meter~")

-- ─────────────────────────────────────
function mcmeter:initialize(_, args)
	self.inlets = { SIGNAL }
	self.outlets = 0
	self.inchans = 4
	self.meter_width = 6
	self.height = 120
	self.frames = 20
	self.width = self.meter_width * self.inchans
	self.neednewrms = true
	self.needdraw = true

	for i, arg in ipairs(args) do
		if arg == "-frame" then
			self.frames = type(args[i + 1]) == "number" and args[i + 1] or 5
		elseif arg == "-width" then
			self.meter_width = type(args[i + 1]) == "number" and args[i + 1] or 4
		elseif arg == "-inchs" then
			self.inchans = type(args[i + 1]) == "number" and args[i + 1] or 1
		elseif arg == "-height" then
			self.height= type(args[i + 1]) == "number" and args[i + 1] or 4
	    end
    end
	self:set_size(self.width, self.height)
	return true
end

-- ─────────────────────────────────────
function mcmeter:update_args()
	local args = {
		"-width",
		self.meter_width,
	}
	table.insert(args, "-height")
	table.insert(args, self.height)
	table.insert(args, "-frames")
	table.insert(args, self.frames)
	table.insert(args, "-inchs")
	table.insert(args, self.inchans)
	self:set_args(args)
end

-- ─────────────────────────────────────
function mcmeter:postinitialize()
	self.clock = pd.Clock:new():register(self, "tick")
	self.clock:delay(0)
end

-- ─────────────────────────────────────
function mcmeter:tick()
	self:repaint(3)
	self.clock:delay(1 / self.frames * 1000)
	self.neednewrms = true
end

-- ─────────────────────────────────────
function mcmeter:in_1_reload()
	self:dofilex(self._scriptname)
	self:initialize("", {})
	self:repaint()
end

-- ─────────────────────────────────────
function mcmeter:in_1_frames(args)
	self.frames = args[1]
	if self.frames < 1 then
		self:error("[mc.meter~] invalid frames value")
		self.frames = 30
		return
	end
	self:update_args()
end

-- ─────────────────────────────────────
function mcmeter:in_1_width(args)
	self.meter_width = args[1]
	if self.meter_width < 1 then
		self:error("[mc.meter~] invalid width value")
		self.meter_width = 1
	end

	self.width = self.meter_width * self.inchans
	if self.width < 8 then
		self:error("[mc.meter~] total width can't be less than 8")
		self.width = 8
		self.meter_width = math.floor(8 / self.inchans + 0.5)
	end
	self:set_size(self.inchans * self.meter_width, self.height)
	self:repaint()
	self:update_args()
end

-- ─────────────────────────────────────
function mcmeter:in_1_height(args)
	self.height = args[1]
	self:set_size(self.inchans * self.meter_width, self.height)
	self:repaint()
	self:update_args()
end

-- ─────────────────────────────────────
function mcmeter:dsp(samplerate, blocksize, inchans)
	self.blocksize = blocksize
	self.inchans = inchans[1]
	self.samplerate = samplerate
	self.width = self.meter_width * self.inchans
	if self.width < 8 then
		self:error("[mc.meter~] total width can't be less than 8")
		self.width = 8
		self.meter_width = math.floor(8 / self.inchans + 0.5)
	end
	self.meters = {}
	for i = 1, self.inchans do
		self.meters[i] = { rms = 0, clipped_warning = false }
	end

	self:set_size(self.width, self.height)
	self:repaint(1)
	self:repaint(2)
end

-- ─────────────────────────────────────
function mcmeter:perform(in1)
	self.invectorsize = self.blocksize * self.inchans
	if not self.neednewrms then
		if #in1 ~= self.invectorsize then
			self.inchans = #in1 / self.blocksize
			self:set_size(self.inchans * self.meter_width, self.height)
		end
		return
	end
	for ch = 1, self.inchans do
		local sum = 0
		local start_idx = (ch - 1) * self.blocksize + 1
		local end_idx = ch * self.blocksize
		for i = start_idx, end_idx do
			sum = sum + in1[i] * in1[i]
		end
		self.meters[ch].rms = math.sqrt(sum / self.blocksize)
	end
	self.neednewrms = false
	self.needdraw = true
end

--╭─────────────────────────────────────╮
--│                PAINT                │
--╰─────────────────────────────────────╯
function mcmeter:paint(g)
	g:set_color(244, 244, 244)
	g:fill_all()
	g:stroke_rect(0, 0, self.width, self.height, 2)
	self:repaint(2)
end

-- ─────────────────────────────────────
function mcmeter:paint_layer_2(g)
	local pos_v = 0

	for _ = 1, self.inchans do
		g:set_color(0, 0, 0)
		g:stroke_rect(pos_v, 0, self.meter_width, self.height, 1)
		pos_v = pos_v + self.meter_width
	end

	pos_v = 0
	for _ = 1, self.inchans do
		g:set_color(80 + 0 * 155, 80 + 1 * 155, 80)
		local green_base_y = self.height - 3
		g:fill_rect(pos_v + 1, green_base_y + 1, self.meter_width - 2, 1)

		pos_v = pos_v + self.meter_width
	end
end

-- ─────────────────────────────────────
function mcmeter:paint_layer_3(g)
	if not self.meters then
		return
	end

	local pos_v = 0
	for ch = 1, self.inchans do
		local rms_value = self.meters[ch].rms or 0
		local clamped_rms = math.min(math.max(rms_value, 0), 1)
		local meter_height = math.min(self.height * clamped_rms, self.height - 2)
		meter_height = math.max(meter_height, 0)
		local fill_y = self.height - meter_height - 1
		local r = math.min(2 * clamped_rms, 1)
		local g_val = math.min(2 * (1 - clamped_rms), 1)
		g:set_color(80 + r * 155, 80 + g_val * 155, 80)
		if meter_height > 0 then
			g:fill_rect(pos_v + 1, fill_y, self.meter_width - 2, meter_height)
		end
		local green_base_y = self.height - 3
		g:fill_rect(pos_v + 1, green_base_y, self.meter_width - 2, 2)
		if meter_height >= 2 then
			g:fill_rect(pos_v + 1, green_base_y, self.meter_width - 2, 1)
		end
		pos_v = pos_v + self.meter_width
	end
end
