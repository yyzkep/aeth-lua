local EngineVGui = {}
EngineVGui.__index = EngineVGui

function EngineVGui.new()
    local self = setmetatable({}, EngineVGui)
    self.instance = engine:get_interface("VEngineVGui002")
    self.paint_addr = engine:get_vfunc(self.instance, 14)
    return self
end

return EngineVGui