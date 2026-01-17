-- scripts/hooks/graphics_hook.lua
local swap_addr = sdl:get_proc_address("SDL_GL_SwapWindow")

if swap_addr ~= "0x0" then
    hook_mgr:add_listener("SDL_GL_SwapWindow", "RenderHeartbeat", function(original, window_ptr)
        render:setup_from_window(window_ptr)
        original(window_ptr)

        if render.ptr then
            render:set_color(255, 0, 0, 255)
            render:rect(20, 20, 150, 150, true)
            
            render:set_color(255, 255, 255, 255)
            render:rect(20, 20, 150, 150, false)
        end
    end, swap_addr)

    hook.trampoline("SDL_GL_SwapWindow", swap_addr)
end