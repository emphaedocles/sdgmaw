local sdgMAWDLL = require("sdgmawix")
local dpsInit = false;
local radarInit = false;
local radarLastCount = 0

radarToCombatlog = false;

local function InitSDGOverlayLog()
    if (sdgMAWDLL) then
         local appPath = AppPath:lower()

        sdgMAWDLL.setlogfile(appPath .. "sdgmawlog.txt")
        sdgMAWDLL.showmsg("Log started..", "SDG- MAW Overlay")
        sdgMAWDLL.showcharstats()
        sdgMAWDLL.setstatustext("V to clear combat log", 0)
        sdgMAWDLL.showdps()
        ShowCombatLog = false
        -- hide in game log when overlay log is used
        if (txtCombatLog) then
            for i = 0, iLastCombatLog - 1 do
                txtCombatLog[i].Active = false
            end

            Game.Redraw = true
        end
        sdgMAWDLL.addline("AppPath:" .. appPath)

        sdgMAWDLL.setbosswarnsound(appPath .. "data\\sounds\\boss.wav")
        sdgMAWDLL.sethiddenwarnsound(appPath .. "data\\sounds\\hidden.wav")
        sdgMAWDLL.setmobappearedsound(appPath .. "data\\sounds\\radar.wav")
        sdgMAWDLL.settreasurerwarnsound(appPath .. "data\\sounds\\treasure.wav")
        sdgMAWDLL.settreasurawnenabled(1)


    end
end
function sdgPlaySound(sound)
    if (sound) then
        local appPath = AppPath:lower()
        sdgMAWDLL.playsound(appPath .. "data\\sounds\\" .. sound)
    end
end
function events.GameInitialized2()

    InitSDGOverlayLog()
end
function SetCurrentMapLevel(name, level)
    if (sdgMAWDLL) then
        local title = "Current Zone:" .. name .. " (Level " .. level .. ")"
        sdgMAWDLL.clsettitle(title)
    end
end
function events.ExitMapAction(t)

    if (t.Action == const.ExitMapAction.MainMenu or t.Action == const.ExitMapAction.NewGame or t.Action == const.ExitMapAction.LoadGame) then
        -- load game or new game
        if (sdgMAWDLL) then
            sdgMAWDLL.addline("New Game or Load Game, refreshing char stats..")
            sdgMAWDLL.newgame()
            sdgMAWDLL.setstatustext("X to clear combat log", 0)
            dpsInit = false
            HideRadar()
            sdgMAWDLL.clearradar()

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
local ticksBetweenCharUpdates = 20
local ticksBetweenRadarUpdates = 100
local radarTicks = 0
local radarVisible = 0
function HideRadar()
    if (sdgMAWDLL) then
        -- and radarVisible == 1) then
        --      if(radarInit) then
        --        radarInit=false
        --        sdgMAWDLL.closeradar()
        --      end
        --        sdgMAWDLL.addline("Hiding radar..")
        --        radarVisible = 0
        --        sdgMAWDLL.setradarvisible(radarVisible)
    end
end
function ShowRadar()
    if (sdgMAWDLL) then
        if (not radarInit) then
            radarInit = true
            sdgMAWDLL.showradar()
            sdgMAWDLL.setproximityrange(512)
            --            radarVisible=1
            --        else
            --            sdgMAWDLL.addline("Showing radar..")
            --            radarVisible = 1
            --            sdgMAWDLL.setradarvisible(radarVisible)
        end
    end
end
function events.Tick()

    updateTicks = updateTicks + 1
    if updateTicks >= ticksBetweenCharUpdates then
        SDGUpdateCharStats()
        updateTicks = 0
    end
    if Game.CurrentScreen ~= 0 then
        HideRadar()
    else
        ShowRadar()
        radarTicks = radarTicks + 1
        if (radarTicks >= ticksBetweenRadarUpdates) then
            radarticks = 0
            UpdateRadar()
        end
    end
end
local function getDistance(p, m)
    return math.sqrt((p.X - m.X) ^ 2 +(p.Y - m.Y) ^ 2 +(p.Z - m.Z) ^ 2)
end
function GetTier(mon)
    local monType =(mon.Id - 1) % 3 + 1
    if mon.NameId >= 220 and mon.NameId < 300 then
        local monsterSkill = string.match(Game.PlaceMonTxt[mon.NameId], "([^%s]+)")
        if monsterSkill == "Omnipotent" then
            monType = 6
        elseif monsterSkill == "Broodling" then
            monType = 5
        else
            monType = 4
        end
    end
    if (monType < 4) then
        monType = 1
    else
        monType = monType - 2
    end
    return monType
end
function UpdateRadar()
    local maxM = 0
    local alive = 0
    local onMap = 0
    local perceptionMaster = 0
    local angle = math.rad(Party.Direction / 2048.0 * 360.0) or 0
    -- local px=math.cos(angle)
    -- local py=math.sin(angle)
    sdgMAWDLL.setpartydir(angle, 0)
    local mapMu = mapvars.completition or 0
    sdgMAWDLL.setmapmu(mapMu / 100.0)
    local maxP = 0;
    if (radarToCombatlog) then
        local msg = "Parsing monsters for radar"
        SDGAddToOverlayLog(msg)
    end
    -- radar is primarly based on Perception, but MonsterID mastery can also help to identify monsters
    local idM_s, idM_m = GetSagecraftSkill(const.Skills.IdentifyMonster)
    local p_s, p_m = GetSagecraftSkill(const.Skills.Perception)
    maxP = p_s * p_m
    maxM = idM_m

    --    for i = 0, Party.High do
    --        local s, m = SplitSkill(Party[i]:GetSkill(const.Skills.IdentifyMonster))
    --        local s1 = SplitSkill(Party[i].Skills[const.Skills.IdentifyMonster])
    --        local sp, mp = SplitSkill(Party[i].Skills[const.Skills.Perception])
    --        if s1 > 0 then
    --            if m > maxM then
    --                maxM = m
    --            end
    --        end
    --        if(sp>0) then
    --            if(mp>perceptionMaster) then
    --                perceptionMaster=mp
    --            end
    --            local perception = sp * mp
    --            if perception > maxP then
    --                maxP = perception
    --            end
    --        end
    --    end
    -- increased skill/mastery in perception will increase the radar range, but it is capped at 4x the base range
    local distAdj = 1 + maxP / 10
    local radarRange = distAdj * 500;

    if radarRange > 4000 then
        radarRange = 4000
    end

    sdgMAWDLL.setradarrange(radarRange);
    -- at Perception Master rank (3), hidden monsters will be detected, but they will be harder to detect, so distance is reduced
    -- at Perception Grandmaster rank (4), hidden monsters will be detected, and distance is not reduced plus Monster Tier will be detected
    for i = 0, Map.Monsters.High do
        local mon = Map.Monsters[i]

        local basehp = mon.HP

        if (baseHP > 0 and not(mon.AIState == 5 or mon.AIState == 11 or mon.AIState == 19)) then
            alive = alive + 1
        end
        local isHidden = 0

        if (mon.ShowOnMap or perceptionMaster >= 3) and mon.ShowAsHostile and baseHP > 0 and not(mon.AIState == 5 or mon.AIState == 11 or mon.AIState == 19) then
            onMap = onMap + 1
            local dx = mon.X - Party.X
            local dy = mon.Y - Party.Y
            local dz = mon.Z - Party.Z

            if (perceptionMaster >= 3 and not mon.ShowOnMap) then
                isHidden = 1
            end

            local dist = math.sqrt(dx * dx + dy * dy +(dz / 2 * dz / 2))

            local detectRange = radarRange
            if (isHidden == 1) then
                -- hidden mosnters are harder to detect, so distance is increased
                if (perceptionMaster < 4) then
                    detectRange = radarRange / 2
                else
                    detectRange = 0
                end
                -- distAdj = dist/4
            end
            if (dist <= detectRange) then
                local tier = GetTier(mon)
                -- 1 normal, 2 boss, 3 brood, 4 omni
                if (isHidden == 1 and perceptionMaster < 4) then
                    tier = 1
                end
                if (maxM < 2) then
                    tier = 1
                    -- require MonID Expert+ to recognized bosses
                end
                if (maxM < 3) then
                    if (tier > 2) then tier = 2 end
                    -- require GM to recognized BL vs Omin bossese
                end
                local exponent = math.floor(mon.Resistances[0] / 1000)
                local hp = mon.HP * 2 ^ exponent
                local basehp = mon.HP
                local fullHP = mon.FullHP * 2 ^ exponent
                local name = Game.MonstersTxt[mon.Id].Name
                if mon.NameId > 0 then
                    name = Game.PlaceMonTxt[mon.NameId]
                end


                sdgMAWDLL.addradarentity(i, dx, dy, dz, tier, isHidden, hp, fullHP, name)
                if (radarToCombatlog) then
                    local msg = string.format("Monster %s:%s, dz:%s ,hidden:%s", name, i, dz, isHidden)
                    SDGAddToOverlayLog(msg)
                end
            else
                sdgMAWDLL.removeradarentity(i);
            end

        else
            sdgMAWDLL.removeradarentity(i);
        end
    end
    if (radarLastCount == 0 and onMap > 0) then
        -- sdgPlaySound("tada.wav")
    end
    radarLastCount = onMap
    radarToCombatlog = false

--    if (p_s > 1) then
--        -- Treasure chests (non-empty only) — IDs offset by 10000 to avoid monster ID collision
--        -- requires perception expert to detect chests and items on ground
----        for i = 0, Map.Sprites.High do
----            local chest = Map.Sprites[i]

----            local hasItems = bit.band(chest.Bits, 16) ~= 0   -- chest.IsChest or chest.IsObeliskChest --and chest.Items and chest.Items.High >= 1
----            if hasItems then
------                local cx=chest.Pos[0]
------                local cy=chest.Pos[1]
------                local cz=chest.Pos[2]
----                local dx = chest.X - Party.X
----                local dy = chest.Y - Party.Y
----                local dz = chest.Z - Party.Z
----                local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
----                if dist <= radarRange then
----                    sdgMAWDLL.addradarentity(10000 + i, dx, dy, dz, 1, 0, 0, 0, "Chest", 1)
----                else
----                    sdgMAWDLL.removeradarentity(10000 + i)
----                end
----            else
----                sdgMAWDLL.removeradarentity(10000 + i)
----            end
----        end

--        -- Ground items (dropped loot) — IDs offset by 20000 to avoid collision
--        --only care about items with a tier > then curren filter
--        local filter=vars.MAWSETTINGS.lootFilter

--		local tierList={"Common", "Uncom.", "Rare", "Epic", "Ancient", "Primordial", "Legendary", [0]="OFF"}
--		local filterPower=table.find(tierList, filter) or 0

--        for i = 0, Map.Objects.High do
--            local obj = Map.Objects[i]
--            local hasItem = obj and obj.Item and obj.Visible and obj.Item.Number and obj.Item.Number > 0 and obj.Item.Bonus  -- or reagentList[obj.Item.Number] then

--            if hasItem then

--                local dx = obj.X - Party.X
--                local dy = obj.Y - Party.Y
--                local dz = obj.Z - Party.Z
--                local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
--                if dist <= radarRange and math.abs(dz)<200 then

--           		local itemPower=1

--		            if obj.Item.Bonus>0 then
--			            itemPower=itemPower+1
--		            end
--		            if obj.Item.Bonus2>0 then
--			            itemPower=itemPower+1
--		            end
--		            if obj.Item.Charges>1000 then
--			            itemPower=itemPower+1
--		            end
--		            if obj.Item.BonusExpireTime==1 then
--			            itemPower=5
--		            elseif obj.Item.BonusExpireTime==2 then
--			            itemPower=6
--		            elseif obj.Item.BonusExpireTime>10 and obj.Item.BonusExpireTime<1000 then
--			            itemPower=7
--		            end

--                    if itemPower >= filterPower then
--                    	local iName= obj.Item:T().Name
--			            local iType = obj.Item:T().NotIdentifiedName
--			            local iDesc= iType .. itemPower
--                        sdgMAWDLL.addradarentity(20000 + i, dx, dy, dz, 1, 0, 0, 0, iDesc, 1)

--                    else
--                        sdgMAWDLL.removeradarentity(20000 + i)
--                    end
--                else
--                    sdgMAWDLL.removeradarentity(20000 + i)
--            end
--            end
--        end
--    end
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

function SDGAddDPSTracking(playerName, damage)

    if sdgMAWDLL then
        if (not dpsInit) then
            SDGInitDPS()
        end
        local tick = Game.Time;
        sdgMAWDLL.adddpsentry(playerName, tick, damage)
    end
end
function SDGInitDPS()
    if sdgMAWDLL and not dpsInit then
        for i = 0, Party.High do
            if Party[i] then
                local char = Party[i]
                sdgMAWDLL.adddpsentry(char.Name, 0)
            end
        end
        dpsInit = true
    end
end
function SDGUpdateCharStats()
    if (sdgMAWDLL) then
        for i = 0, Party.High do
            if Party[i] then
                local char = Party[i]
                DPS1, DPS2, DPS3, vitality = calcPowerVitality(char, true)
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
                local mrating = DPS1
                -- shortenNumber(DPS1, 4, true)
                local rrating = DPS2
                -- shortenNumber(DPS2,4,true)
                local srating = DPS3
                -- shortenNumber(DPS3,4,true)
                local vr = vitality
                -- shortenNumber(vitality, 4, true)
                local debuffs = ""
                if (char.Dead > 0) then
                    debuffs = debuffs .. "Dead "
                end
                if (char.Poison1 > 0 or char.Poison2 > 0 or char.Poison3 > 0) then
                    debuffs = debuffs .. "Poisoned "
                end
                if (char.Cursed > 0) then
                    debuffs = debuffs .. "Cursed "
                end
                if (char.Weak > 0) then
                    debuffs = debuffs .. "Weakened "
                end
                if (char.Asleep > 0) then
                    debuffs = debuffs .. "Asleep "
                end
                if (char.Paralyzed > 0) then
                    debuffs = debuffs .. "Paralyzed "
                end
                if (char.Stoned > 0) then
                    debuffs = debuffs .. "Petrified "
                end
                if (char.Eradicated > 0) then
                    debuffs = debuffs .. "Eradicated "
                end
                if (char.Disease1 > 0 or char.Disease2 > 0 or char.Disease3 > 0) then
                    debuffs = debuffs .. "Diseased "
                end
                if (char.Insane > 0) then
                    debuffs = debuffs .. "Insane "
                end
                if (debuffs == "") then
                    statusfx = "Good"
                else
                    statusfx = debuffs
                end

                local id = Party[i]:GetIndex()
                mapvars.damageTrackRanged = mapvars.damageTrackRanged or { }
                mapvars.damageTrackRanged[id] = mapvars.damageTrackRanged[id] or 0

                mapvars.damageTrack = mapvars.damageTrack or { }
                mapvars.damageTrack[id] = mapvars.damageTrack[id] or 0

                mapvars.damageTrackRanged = mapvars.damageTrackRanged or { }
                mapvars.damageTrackRanged[id] = mapvars.damageTrackRanged[id] or 0

                local mapmelee = shortenNumber(mapvars.damageTrack[id], 3)
                local mapranged = shortenNumber(mapvars.damageTrackRanged[id], 3)
                local mapTotal = shortenNumber(mapvars.damageTrack[id] + mapvars.damageTrackRanged[id], 3)
                -- heal totals
                local tot1 = 0
                local tot2 = 0
                local tot3 = 0
                mapvars.regenerationHeal = mapvars.regenerationHeal or { }
                mapvars.regenerationHeal[id] = mapvars.regenerationHeal[id] or 0

                mapvars.healingDone = mapvars.healingDone or { }
                mapvars.healingDone[id] = mapvars.healingDone[id] or 0

                mapvars.leechDone = mapvars.leechDone or { }
                mapvars.leechDone[id] = mapvars.leechDone[id] or 0


                tot1 = mapvars.healingDone[id] or 0
                tot2 = mapvars.regenerationHeal[id] or 0
                tot3 = mapvars.leechDone[id] or 0

                tot1 = math.max(tot1, 0)
                tot2 = math.max(tot2, 0)
                tot3 = math.max(tot3, 0)
                local tot4 =(tot1 + tot2 + tot3) or 0

                local mapheals = shortenNumber(tot4, 3)

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
                sdgMAWDLL.setchardetails(char.Name, Game.ClassNames[char.Class], char.LevelBase, char.HP, maxHP, mp, manaPool, fullSP, 0, 0, ac, statusfx, mrating, rrating, srating, vr, mapmelee, mapranged, mapheals, mapTotal)
            end
        end
    end
end
