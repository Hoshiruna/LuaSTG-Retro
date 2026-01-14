--------------------------------------------------------------------------------
--- PMD playback example
--------------------------------------------------------------------------------

local function load_pmd()
    -- Requires LUASTG_AUDIO_PMDWIN_ENABLE=ON and PMD/PPZ/P86 assets.

    -- Use the legacy LuaSTG module for resource and audio helpers.
    local lstg = require("lstg")

    -- Resource name and file path for the PMD track.
    local pmd_name = "bgm:pmd_demo"
    local pmd_path = "res/music/demo.pmd"
    -- Loop settings are in seconds; 0 disables explicit loop sections.
    local loop_end = 0.0
    local loop_length = 0.0

    function GameInit()
        -- The PMD decoder looks for PCM packs (.p86/.ppz) next to the .pmd or in search paths.
        lstg.LoadMusic(pmd_name, pmd_path, loop_end, loop_length)
        -- Start playback at full volume from the beginning.
        lstg.PlayMusic(pmd_name, 1.0, 0.0)
    end

    function FrameFunc()
        -- Return false to keep the game running.
        return false
    end

    function RenderFunc()
    end

    function GameExit()
        -- Stop the music when the game exits.
        lstg.StopMusic(pmd_name)
    end
end
