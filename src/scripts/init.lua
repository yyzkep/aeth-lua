-- scripts/init.lua
local HookManager = {}
HookManager.__index = HookManager

function HookManager.new()
    local self = setmetatable({}, HookManager)
    self.handlers = {} 
    self.max_errors = 5
    return self
end

local function printf(fmt, ...) 
    print(string.format("[aeth] " .. fmt, ...)) 
end

local aeth_path = os.getenv("AETH_PATH") or "."
local paths = { "/scripts/?.lua", "/scripts/sdk/?.lua", "/scripts/functions/?.lua" }
for _, p in ipairs(paths) do
    package.path = package.path .. ";" .. aeth_path .. p
end

function HookManager:add_listener(event_name, listener_name, func)
    if type(func) ~= "function" then 
        printf("'%s' failed to register: not a function", listener_name)
        return 
    end

    self.handlers[event_name] = self.handlers[event_name] or {}
    table.insert(self.handlers[event_name], { 
        name = listener_name, 
        run = func, 
        error_count = 0,
        active = true 
    })
    printf("registered '%s' to event '%s'", listener_name, event_name)
end

function HookManager:execute(hook_name, ...)
    local args = {...}
    local listeners = self.handlers[hook_name]
    
    local original = function(...)
        local call_args = {...}
        if #call_args == 0 then
            hook.call_original(hook_name, table.unpack(args))
        else
            hook.call_original(hook_name, table.unpack(call_args))
        end
    end

    if not listeners or #listeners == 0 then
        original()
        return
    end

    for _, listener in ipairs(listeners) do
        if listener.active then
            local success, err = pcall(listener.run, original, table.unpack(args))
            
            if not success then
                listener.error_count = (listener.error_count or 0) + 1
                if listener.error_count >= self.max_errors then
                    listener.active = false
                end
            end
        end
    end
end

_G.hook_mgr = HookManager.new()
_G.i_engine = require("engine_client").new()
_G.i_vgui   = require("engine_vgui").new()

function hook.event_bus(hook_name, ...)
    hook_mgr:execute(hook_name, ...)
end

require("test")
require("hooks/graphics_hook")

printf("system initialized successfully.")