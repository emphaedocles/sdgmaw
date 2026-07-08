
-- rejuve (HoT) replace Life Detect test
local rejuveTimeDelta = 30-- how often to update hot

function events.GameInitialized2()
    InitRejuvs()
end
local scalingArrayRejuv = { 6, 9, 12, 15 } -- amount increased per point of spirit for each mastery level
local baseArrayRejuv = { 20, 30, 40, 50 } -- base heal for each mastery level
local costArrayRejuv = { 18, 45, 72, 120 } -- mana cost for each mastery level
function InitRejuvs()

    Game.Spells[45]["Name"] = "Rejuvenation"
    Game.Spells[45]["Description"] = "Casts a Heal over time effect on the whole party, healing for more the higher the caster's spirit skill is. Heals for 2 minutes.\n Modified by Ascenscion"
    Game.Spells[45]["ShortName"] = "Rejuv"
    Game.Spells[45]["SpellPointsNormal"] = 18
    Game.Spells[45]["SpellPointsExpert"] = 45
    Game.Spells[45]["SpellPointsMaster"] = 72
    Game.Spells[45]["SpellPointsGM"] = 120
    -- todo add all malekith's healing scaling/ascencion etc....
    Game.SpellsTxt[45].Normal = string.format("%s Mana cost: \ncures  20 HP per point of skill over 5 minutes to all party members", 18)
    Game.SpellsTxt[45].Expert = string.format("%s Mana cost: \ncures  30 HP per point of skill over 5 minutes to all party members", 45)
    Game.SpellsTxt[45].Master = string.format("%s Mana cost: \ncures  40 HP per point of skill over 5 minutes to all party members", 72)
    Game.SpellsTxt[45].GM = string.format("%s Mana cost: \ncures  50 HP per point of skill over 5 minutes to all party members", 120)



    --    vars.rejuvTrack = vars.rejuvTrack or { }
    --    for i = 0, Party.High do
    --        vars.rejuvTrack[i] = vars.rejuvTrack[i] or { }
    --        vars.rejuvTrack[i]["HPS"] = 0
    --        vars.rejuvTrack[i]["Expiry"] = 0--in seconds Game.Time
    --        vars.rejuvTrack[i]["LastTime"] = 0
    --        vars.rejuvTrack[i]["HealLeft"]=0
    --    end
end
function events.PlayerCastSpell(t)
    if t.SpellId == 45 then

        local pl = t.Player
        local s, m = SplitSkill(pl:GetSkill(const.Skills.Spirit))
        if s > 0 then
            local level = pl:GetSkill(const.Skills.Learning)
            local sa, ma = SplitSkill(level)

            local personalityReduction = getPersonalityManaCostReduction(pl)
            local cost = t.SPCost
            cost=cost*(1+sa*0.125)*1.04^(sa)*(1-0.125*ma)

            cost = math.min(math.ceil(cost * personalityReduction), 65000)
            if t.Player.SP < cost then
                return
            end

            
            local healAmount =0
            local healMult = getHealSpellMultiPlier(pl)
            -- function ascendSpellHealing(skill, mastery, spell, healM)
            local level = pl:GetSkill(const.Skills.Learning)
            local sa, ma = SplitSkill(level)

            base = baseArrayRejuv[m]
            scaling = scalingArrayRejuv[m]
            scaling = scaling *(1 + 0.06 * sa) * 1.02 ^ sa
            base = base *(1 + 0.025 * sa ^ 2) * 1.02 ^ sa
            scaling, base = round(scaling), round(base)
            healAmount = base + scaling * s
            -- cost*(1+s*0.125)*1.04^(s)*(1-0.125*m)
            -- return scaling, base
            -- end

            healAmount = math.floor(healAmount * healMult)
            pl.SP = pl.SP - cost

            local duration = 300
            local hps = healAmount / duration
            AddCombatLog(pl.Name .. " casts Rejuvenation on party" .. string.format(" for %s over %s", healAmount, duration .. " seconds"))
            vars.rejuvTrack = vars.rejuvTrack or { }
            for i = 0, Party.High do
                vars.rejuvTrack[i] = vars.rejuvTrack[i] or { }
                vars.rejuvTrack[i]["HPS"] = hps
                vars.rejuvTrack[i]["Expiry"] = Game.Time + duration * const.Second
                vars.rejuvTrack[i]["LastTime"] = Game.Time
                vars.rejuvTrack[i]["HealLeft"] = healAmount
                local plTarget = Party[i]
                local healTick = hps * rejuveTimeDelta
                plTarget.HP = math.min(plTarget.HP + healTick, GetMaxHP(plTarget))
                -- heal for first tick on cast
                AddHealToLog("Rejuvenation", healTick, false, i, pl)
                SetHealTextReset(i, healTick)

            end


            t.Handled = true
        end
    end
end

function events.Tick()
    local gTime = Game.Time * const.Second
    -- get game time in seconds

    -- Rejuvenation HoT ticks
    vars.rejuvTrack = vars.rejuvTrack or { }
    for i = 0, Party.High do
        local plTarget = Party[i]
        local fullhp = GetMaxHP(plTarget)
        if vars.rejuvTrack[i] and vars.rejuvTrack[i]["Expiry"] and Game.Time >= vars.rejuvTrack[i]["Expiry"] then

            if (vars.rejuvTrack[i]["Expiry"] ~= 0) then
                -- determine if any left overhealing to do
                if (vars.rejuvTrack[i]["HealLeft"] > 0) then
                    local healTick = vars.rejuvTrack[i]["HealLeft"]

                    local amountHealed = math.min(healTick, fullhp - plTarget.HP)

                    if ((amountHealed) and(Party.EnemyDetectorYellow or Party.EnemyDetectorRed)) then
                        local id = Party[i]:GetIndex()
                        mapvars.healingDone = mapvars.healingDone or { }
                        mapvars.healingDone[id] = mapvars.healingDone[id] or 0
                        mapvars.healingDone[id] = mapvars.healingDone[id] + amountHealed
                        vars.healingDone = vars.healingDone or { }
                        vars.healingDone[id] = vars.healingDone[id] or 0
                        vars.healingDone[id] = vars.healingDone[id] + amountHealed
                    end

                    plTarget.HP = math.min(plTarget.HP + healTick, fullhp)
                    -- heal for last tick on expiry if any heal left

                    AddHealToLogX("Rejuvenation", healTick, false, i)
                    SetHealTextReset(i, healTick)
                end
                AddCombatLog("Rejuvenation on " .. Party[i].Name .. " has expired.")
            end
            vars.rejuvTrack[i]["HPS"] = 0
            vars.rejuvTrack[i]["Expiry"] = 0
            vars.rejuvTrack[i]["LastTime"] = 0
            vars.rejuvTrack[i]["HealLeft"] = 0
        elseif vars.rejuvTrack[i] and vars.rejuvTrack[i]["HPS"] and vars.rejuvTrack[i]["HPS"] > 0 then

            local timePassed =(Game.Time - vars.rejuvTrack[i]["LastTime"]) / const.Second
            if timePassed >=(rejuveTimeDelta) or(Game.TurnBased and Game.TurnBasedPhase == 3) then

                local healAmount = vars.rejuvTrack[i]["HPS"] *(timePassed)
                if (healAmount >= 1) then
                    -- AddCombatLog("Time Passed:" ..timePassed)
                    local amountHealed = math.min(healAmount, fullhp - plTarget.HP)

                    if (amountHealed > 0 and(Party.EnemyDetectorYellow or Party.EnemyDetectorRed)) then
                        local id = Party[i]:GetIndex()
                        mapvars.healingDone = mapvars.healingDone or { }
                        mapvars.healingDone[id] = mapvars.healingDone[id] or 0
                        mapvars.healingDone[id] = mapvars.healingDone[id] + amountHealed
                        vars.healingDone = vars.healingDone or { }
                        vars.healingDone[id] = vars.healingDone[id] or 0
                        vars.healingDone[id] = vars.healingDone[id] + amountHealed
                    end

                    Party[i].HP = math.min(Party[i].HP + healAmount, fullhp)
                    AddHealToLogX("Rejuvenation", healAmount, false, i)
                    SetHealTextReset(i, healAmount)
                    vars.rejuvTrack[i]["HealLeft"] = math.max(vars.rejuvTrack[i]["HealLeft"] - healAmount, 0)
                    vars.rejuvTrack[i]["LastTime"] = Game.Time
                end
            end
        end
    end
end