local EngineClient = require("engine_client")
_G.i_engine = EngineClient.new()

_G.i_engine:execute_command("clear");

if _G.i_engine:is_in_game() then
    local me = _G.i_engine:get_local_player()
    print("[AETH] Currently in game. Local index: " .. me)
end