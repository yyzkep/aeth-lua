local EngineClient = {}
EngineClient.__index = EngineClient

function EngineClient.new()
    local self = setmetatable({}, EngineClient)
    self.instance = engine:get_interface("VEngineClient013")
    return self
end

function EngineClient:is_in_game()
    if self.instance == "0x0" then return false end
    local addr = engine:get_vfunc(self.instance, 26)
    return mem.call_bool(addr, self.instance)
end

function EngineClient:get_local_player()
    if self.instance == "0x0" then return 0 end
    local addr = engine:get_vfunc(self.instance, 12)
    return mem.call_int(addr, self.instance)
end

function EngineClient:execute_command(cmd)
    if self.instance == "0x0" then return end
    local addr = engine:get_vfunc(self.instance, 106)
    mem.call_void_str(addr, self.instance, cmd)
end

return EngineClient