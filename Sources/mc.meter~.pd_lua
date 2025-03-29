local meter = pd.Class:new():register("mc.meter~")

-- ─────────────────────────────────────
function meter:initialize(name, args)
	self.inlets = { SIGNAL }
	self.outlets = 0
	self.inchans = 4
	self.meter_width = 6
	self.height = 80
	self.frames = 60
	self.width = self.meter_width * self.inchans
	self:set_size(self.width, self.height)
	self.neednewrms = true

	for i, arg in ipairs(args) do
		if arg == "-frame" then
			self.frames = type(args[i + 1]) == "number" and args[i + 1] or 5
		elseif arg == "-width" then
			self.meter_width = type(args[i + 1]) == "number" and args[i + 1] or 4
		elseif arg == "-inchs" then
			self.inchans = type(args[i + 1]) == "number" and args[i + 1] or 1
		end
	end
	return true
end

-- ─────────────────────────────────────
function meter:update_args()
	local args = {
		"-width",
		self.meter_width,
	}
	table.insert(args, "-frames")
	table.insert(args, self.frames)
	table.insert(args, "-inchs")
	table.insert(args, self.inchans)
	self:set_args(args)
end

-- ─────────────────────────────────────
function meter:postinitialize()
	self.clock = pd.Clock:new():register(self, "tick")
	self.clock:delay(0)
end

-- ─────────────────────────────────────
function meter:tick()
	self:repaint(3)
	self.clock:delay(1 / self.frames * 1000)
	self.neednewrms = true
end

-- ─────────────────────────────────────
function meter:in_1_reload()
	self:dofilex(self._scriptname)
	self:initialize("", {})
	self:repaint()
end

-- ─────────────────────────────────────
function meter:in_1_frames(args)
	self.frames = args[1]
	self:update_args()
end
-- ─────────────────────────────────────
function meter:in_1_width(args)
	self.meter_width = args[1]
	self:set_size(self.inchans * self.meter_width, self.height)
	self:repaint()
	self:repaint(1)
	self:repaint(2)
	self:repaint(3)
	self:update_args()
end

-- ─────────────────────────────────────
function meter:dsp(samplerate, blocksize, inchans)
	self.blocksize = blocksize
	self.inchans = inchans[1]
	self.samplerate = samplerate
	self.width = self.meter_width * self.inchans
	self:set_size(self.width, 80)
	self:repaint(1)
	self:repaint(2)
end

-- ─────────────────────────────────────
function meter:perform(in1)
	self.invectorsize = self.blocksize * self.inchans
	if not self.neednewrms then
		if #in1 ~= self.invectorsize then
			self.inchans = #in1 / self.blocksize
			self:set_size(self.inchans * self.meter_width, self.height)
		end
	end

	self.rms = {} -- Initialize/reset RMS table
	for ch = 1, self.inchans do
		local sum = 0
		local start_idx = (ch - 1) * self.blocksize + 1 -- Start index for this channel
		local end_idx = ch * self.blocksize -- End index for this channel

		-- Compute RMS for the channel
		for i = start_idx, end_idx do
			sum = sum + in1[i] * in1[i] -- Square each sample and sum
		end

		self.rms[ch] = math.sqrt(sum / self.blocksize) -- Final RMS calculation
	end
	self.neednewrms = false
end

--╭─────────────────────────────────────╮
--│                PAINT                │
--╰─────────────────────────────────────╯
function meter:paint(g)
	g:set_color(244, 244, 244)
	g:fill_all()
end

-- ─────────────────────────────────────
function meter:paint_layer_2(g)
	local pos_v = 0
	for i = 1, self.inchans do
		g:set_color(0, 0, 0)
		g:stroke_rect(pos_v, 0, self.meter_width, self.height, 1)
		pos_v = pos_v + self.meter_width
	end
end

-- ─────────────────────────────────────
function meter:paint_layer_3(g)
	if not self.rms then
		return
	end

	local pos_v = 0
	for ch = 1, self.inchans do
		local rms_value = self.rms[ch] or 0
		local meter_height = (self.height * 0.9) * rms_value
		if meter_height < 2 then
			meter_height = 2
		end
		local r = math.min(2 * rms_value, 1) -- Increases from 0 → 1
		local g_val = math.min(2 * (1 - rms_value), 1) -- Decreases from 1 → 0

		g:set_color(80 + r * 155, 80 + g_val * 155, 100)
		g:fill_rect(pos_v + 1, self.height - meter_height, self.meter_width - 2, meter_height)

		pos_v = pos_v + self.meter_width -- Move to the next meter
	end
end
