-- scripts/sdk/renderer.lua
local Renderer = {}
Renderer.__index = Renderer

function Renderer.new()
    local self = setmetatable({}, Renderer)
    
    self.fns = {
        GetRenderer  = sdl:get_proc_address("SDL_GetRenderer"),
        SetDrawColor = sdl:get_proc_address("SDL_SetRenderDrawColor"),
        DrawLine     = sdl:get_proc_address("SDL_RenderDrawLine"),
        DrawRect     = sdl:get_proc_address("SDL_RenderDrawRect"),
        FillRect     = sdl:get_proc_address("SDL_RenderFillRect"),
    }
    
    self.ptr = nil
    return self
end

function Renderer:setup_from_window(window_ptr)
    if self.fns.GetRenderer ~= "0x0" then
        self.ptr = mem.call(self.fns.GetRenderer, window_ptr)
    end
end

function Renderer:setup(renderer_ptr)
    self.ptr = renderer_ptr
end

function Renderer:set_color(r, g, b, a)
    if not self.ptr or self.ptr == "0x0" then return end
    mem.call(self.fns.SetDrawColor, self.ptr, r, g, b, a)
end

function Renderer:line(x1, y1, x2, y2)
    if not self.ptr or self.ptr == "0x0" then return end
    mem.call(self.fns.DrawLine, self.ptr, x1, y1, x2, y2)
end

function Renderer:rect(x, y, w, h, filled)
    if not self.ptr or self.ptr == "0x0" then return end
    
    -- typedef struct SDL_Rect
    -- { int x, y; int w, h; SDL_Rect; } 
    local rect_addr = mem.alloc(16)
    
    mem.write_int(rect_addr, 0,  x)
    mem.write_int(rect_addr, 4,  y)
    mem.write_int(rect_addr, 8,  w)
    mem.write_int(rect_addr, 12, h)

    if filled then
        mem.call(self.fns.FillRect, self.ptr, rect_addr)
    else
        mem.call(self.fns.DrawRect, self.ptr, rect_addr)
    end
    
    mem.free(rect_addr)
end

_G.render = Renderer.new()
return _G.render