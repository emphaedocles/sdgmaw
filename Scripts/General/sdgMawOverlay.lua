local sdgMAWDLL = require("sdgmawix")--  mem.dll[DevPath .. "Scripts/DLL/sdgMawOverlayCX.dll"]
----local sdgMAWDLL = package.loadlib("D:/games/MnMMaw4/mm8_quckstart/Scripts/DLL/sdgmawoverlaycx.dll", "luaopen_sdgmawoverlaycx")
----local sdgMAWDLL= mem.dll[DevPath .. "Scripts/DLL/sdgmawoverlaycx.dll"]

function InitSDGOverlayLog()
    if (sdgMAWDLL) then
        ----debug.Message(sdgMAWDLL)
        sdgMAWDLL.showmsg("Log started..", "SDG- MAW Overlay")
        sdgMAWDLL.showcharstats()
        sdgMAWDLL.setstatustext("V to clear combat log",0)
        ShowCombatLog = false
        -- hide in game log when overlay log is used
        if (txtCombatLog) then
            for i = 0, iLastCombatLog - 1 do
                txtCombatLog[i].Active = false
            end

            Game.Redraw = true
        end
        -- sdgMAWDLL.showmsg("Tick", "for tock")

        ----for index, value in ipairs(sdgMAWDLL) do
        ----  debug.Message(index .. ": " .. value)
        ----end
        --        --sdgMAWDLL.addtext("hello world")
    end
end
function events.ExitMapAction(t)

  if(t.Action == const.ExitMapAction.MainMenu or t.Action == const.ExitMapAction.NewGame or t.Action == const.ExitMapAction.LoadGame) then
   --load game or new game
   if(sdgMAWDLL) then
    sdgMAWDLL.addline("New Game or Load Game, refreshing char stats..")
    sdgMAWDLL.newgame()
    sdgMAWDLL.setstatustext("V to clear combat log",0)

   end
  end
end
function SDGAddToOverlayLog(msg)
    if (sdgMAWDLL) then
        sdgMAWDLL.addline(msg)
    end
end
function SDGClearLog()
    if (sdgMAWDLL) then
        sdgMAWDLL.clearlog()
    end
end
function SDGShowCharStats()
    if (sdgMAWDLL) then
        sdgMAWDLL.showcharstats()
    end
end
local updateTicks = 0
local ticksBetweenCharUpdates = 10
function events.Tick()
    updateTicks = updateTicks + 1
    if updateTicks >= ticksBetweenCharUpdates then
        SDGUpdateCharStats()
        updateTicks = 0
    end
end
local function GetSPRegen(char)
    local fullSP = char:GetFullSP()
    local i = char:GetIndex()
    if vars.MAWSETTINGS.buffRework == "ON" and vars.currentManaPool and vars.currentManaPool[i] then
        fullSP = fullSP *(vars.currentManaPool[i] / fullSP) ^ 0.5
    end
    local skill = char:GetSkill(const.Skills.Meditation)
    local s, m = SplitSkill(skill)
    if m == 4 then
        m = 5
    end
    local medRegen = round(fullSP ^ 0.35 * s ^ 1.4 *(m + 1) / 20) + 2
    -- meditation buff
    if vars.MAWSETTINGS.buffRework == "ON" and vars.mawbuff[56] then
        local s, m, level = getBuffSkill(56)
        local level = level ^ 0.65
        medRegen = medRegen + round((fullSP ^ 0.35 * level ^ 1.4 *((buffPower[56].Base[m]) / 100) + 10) *(1 + buffPower[56].Scaling[m] / 100 * s))
    end

    local SPregenItem = 0
    local bonusregen = 0
    for it in char:EnumActiveItems() do
        if it.Bonus2 == 38 or it.Bonus2 == 47 or it.Bonus2 == 55 or it.Bonus2 == 66 then
            -- SPregenItem=SPregenItem+1
            -- bonusregen=1
            -- such enchants now increase meditation instead
        end
        if table.find(artifactSpRegen, it.Number) then
            SPregenItem = SPregenItem + 1
            bonusregen = 1
        end
    end
    regen = math.ceil(fullSP * SPregenItem * 0.01) + medRegen + bonusregen
    return regen
end
function SDGUpdateCharStats()
    if (sdgMAWDLL) then
        for i = 0, Party.High do
            if Party[i] then
                local char = Party[i]
                DPS1, DPS2, DPS3, vitality = calcPowerVitality(char,true)
                local ac = Party[i]:GetArmorClass()

                local regen = 0
                -- math.round(getBuffHealthRegen(char) * 10) / 10
                local spregen = 0
                -- GetSPRegen(char);
                local maxHP = GetMaxHP(char)
                local mp = char.SP
                local manaPool = vars.currentManaPool[i]
                local fullSP = vars.maxManaPool[i]

                local statusfx;
                local mrating =DPS1-- shortenNumber(DPS1, 4, true)
                local rrating =DPS2-- shortenNumber(DPS2,4,true)
                local srating =DPS3-- shortenNumber(DPS3,4,true)
                local vr =vitality-- shortenNumber(vitality, 4, true) 
                local debuffs=""
                if( char.Dead>0)then
                    debuffs=debuffs.."Dead "
                end
                if(char.Poison1>0 or char.Poison2>0 or char.Poison3>0)then
                    debuffs=debuffs.."Poisoned "
                end
                if(char.Cursed>0)then
                    debuffs=debuffs.."Cursed "
                end
                if(char.Weak>0) then
                    debuffs=debuffs.."Weakened "
                end
                if(char.Asleep>0) then
                    debuffs=debuffs.."Asleep "
                end
                if(char.Paralyzed>0) then
                    debuffs=debuffs.."Paralyzed "
                end
                if(char.Stoned>0) then
                    debuffs=debuffs.."Petrified "
                end
                if(char.Eradicated>0) then
                    debuffs=debuffs.."Eradicated "
                end
                if(char.Disease1>0 or char.Disease2>0 or char.Disease3>0) then
                    debuffs=debuffs.."Diseased "
                end
                if(char.Insane>0)then
                    debuffs=debuffs.."Insane "
                end
                if(debuffs=="")then
                    statusfx="Good"
                else
                    statusfx=debuffs
                end

                --          local stats = {
                --            ["Name"] = char.Name,
                --            ["Class"] = char.Class,
                --            ["Level"] = char.LevelBase,
                --            ["HP"] = char.HP,
                --            ["MaxHP"] = GetMaxHP(char),
                --            ["MP"] = char.MP,
                --            ["MaxMP"] =0,-- char.GetFullSP(),
                --            ["HPRegen"] = regen,
                --            ["SPRegen"] = spregen
                --            }
                sdgMAWDLL.setchardetails(char.Name, Game.ClassNames[char.Class], char.LevelBase, char.HP, maxHP, mp, manaPool, fullSP, 0, 0, ac, statusfx, mrating, rrating, srating, vr)
            end
        end
    end
end
