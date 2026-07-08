----function events.Action(t)
----    if t.Action == 105 and Game.CurrentPlayer >= 0 and Game.CurrentPlayer <= Party.High then
----        AddCombatLog("Checking Assassin Spells- Phae code")

----        pl = Party[Game.CurrentPlayer]
----        local id = pl:GetIndex()
----        --        --debug before
----        --        for key, value in pairs(assassinSpells) do
----        --            AddCombatLog(Game.SpellsTxt[key].Name .. " cost " .. value.Cost .. " stack cost " .. value.StackCost .. " damage multiplier " .. value.DamageMult)
----        --        end
----        vars.ShadowStepAdded = vars.ShadowStepAdded or { }
----        vars.ShadowStepAdded[id] = vars.ShadowStepAdded[id] or false
----        if table.find(assassinClass, pl.Class) then
----            local s3, m3 = SplitSkill(pl.Skills[const.Skills.Water])
----            -- poisons
----            local sp = const.Spells
----            if (m3 >= 1) then
----                if (not vars.ShadowStepAdded[id]) then
----                    AddCombatLog(pl.Name .. " has learned Shadow Step!")
----                    assassinSpellList[const.Skills.Water] = assassinSpellList[const.Skills.Water] or { }
----                    table.insert(assassinSpellList[const.Skills.Water], 28)


----                    assassinSpells[sp.IceBolt] = { ["Cost"] = 0, ["StackCost"] = 1, ["DamageMult"] = 2, }

----                    -- table.insert(assassinSpells, [sp.IceBolt]={["Cost"]=0,["StackCost"]=5,["DamageMult"]=2,})
----                    vars.ShadowStepAdded[id] = true
----                    --                    --debug after
----                    --                    for key, value in pairs(assassinSpells) do
----                    --                        AddCombatLog(Game.SpellsTxt[key].Name .. " cost " .. value.Cost .. " stack cost " .. value.StackCost .. " damage multiplier " .. value.DamageMult)
----                    --                    end

----                end
----                pl.Spells[sp.IceBolt] = true
----                --                --debug
----                --                for i in pl.Spells do
----                --                   if(pl.Spells[i]) then
----                --                        AddCombatLog("Player spell: " .. Game.SpellsTxt[i].Name)
----                --                    end
----                --                end
----            end
----        end
----    end
----end




----local function UpdateNewAssSkills(isAssassin, pl)
----    if isAssassin then
----        if pl then
----            for key, value in pairs(assassinSpells) do
----                local id = pl:GetIndex()
----                if vars.assassinStacks[id] < assassinSpells[key].StackCost then
----                    for i = 1, 4 do
----                        Game.Spells[key]["SpellPoints" .. masteryName[i]] = 1000
----                    end
----                else
----                    for i = 1, 4 do
----                        Game.Spells[key]["SpellPoints" .. masteryName[i]] = assassinSpells[key].Cost
----                    end
----                end
----            end
----        end
----        AddCombatLog("Assassin Spells Updated for " .. pl.Name)
----        -- skill names and desc
----        local sp = const.Spells

----        Game.SpellsTxt[sp.IceBolt].Description = string.format("Shadow Step, allows the assassin and their companions to step through the shadows within melee range of their target.\nShadow Step does %s%% of a melee attack damage.", assassinSpells[sp.IceBolt].DamageMult * 100)
----        Game.SpellsTxt[sp.IceBolt].ShortName = "Shadow Step"
----        Game.SpellsTxt[sp.IceBolt].Name = "Shadow Step"
----        Game.SpellsTxt[sp.IceBolt].Normal = "N/A"
----        Game.SpellsTxt[sp.IceBolt].Expert = "N/A"

----        Game.SpellsTxt[sp.IceBolt].Master = "Shadow Step has a range of roughly 3x melee range"
----        Game.SpellsTxt[sp.IceBolt].GM = "Shadow Step has a range of roughly 5x melee range. Has chance to stun"

----        Game.SpellsTxt[sp.IceBolt].Description = Game.SpellsTxt[sp.IceBolt].Description .. "\n\nThis Ability requires " .. assassinSpells[sp.IceBolt].StackCost .. " Combo Points to be casted."


----    end
----end
----local function UpdateNewSk(id)
----    UpdateNewAssSkills(false)
----    -- clear changes
----    if id >= 0 and id <= Party.High then
----        local class = Party[id].Class
----        if table.find(assassinClass, class) then
----            UpdateNewAssSkills(true, Party[id])
----            return
----        end
----    end
----end
----function events.Action(t)

----    if t.Action == 114 then
----        UpdateNewSk(Game.CurrentPlayer)
----    end
----    if t.Action == 110 then
----        UpdateNewSk(t.Param - 1)
----    elseif t.Action == 176 then
----        local current = Game.CurrentPlayer
----        local maxParty = Game.Party.High
----        for i = 1, Party.Count do
----            newSelected = current + i + 1
----            while newSelected > maxParty do
----                newSelected = newSelected - Party.Count
----            end
----            local pl = Party[newSelected]
----            if pl.Dead == 0 and pl.Stoned == 0 and pl.Paralyzed == 0 and pl.Eradicated == 0 and pl.Asleep == 0 and pl.Unconscious == 0 then
----                UpdateNewSk(newSelected)
----            end
----        end
----    end

----end
----local function calculateDirection(x_m, y_m, x_p, y_p)
----    local deltaX = x_p - x_m
----    local deltaY = y_p - y_m
----    local mag = math.sqrt(deltaX * deltaX + deltaY * deltaY)
----    local normalizedX = deltaX / mag
----    local normalizedY = deltaY / mag

----    local theta = math.atan2(deltaY, deltaX)
----    -- Calculate the angle in radians
----    local direction = math.floor((theta /(2 * math.pi)) * 2048) % 2048
----    return direction, normalizedX, normalizedY
----end
------ function events.PlayerCastSpell(t)
------    local sp = const.Spells
------    if t.SpellId == sp.IceBolt then
------        -- shadow step

------        local pl = t.Player
------        if table.find(assassinClass, pl.Class) then


------            local s, m = SplitSkill(pl:GetSkill(const.Skills.Water))
------            local meleeRange = 328
------            local range = meleeRange * 3
------            if (m >= 4) then
------                range = meleeRange * 5
------            end
------            local target = pl.Target
------            if target and target.Type == const.TargetType.Monster then
------                local dx = mon.X - Party.X
------                local dy = mon.Y - Party.Y
------                local dz = mon.Z - Party.Z
------                local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
------                local nx = dx / dist
------                local ny = dy / dist
------                local nz = dz / dist
------                if dist > range then
------                    AddCombatLog("Target is out of range for Shadow Step")
------                    return false
------                else
------                    Game.ShowStatusText("Shadow Step!")
------                    AddCombatLog("Shadow Step used on " .. target.Name)

------                end
------            end
------        end
------    end
------ end
----function events.GameInitialized2()
----    function events.CalcDamageToMonster(t)
----        --AddCombatLog("CalcDamageToMonster Shadow Step start")

----        local data = WhoHitMonster()
----        if (data and data.Player) then
----            -- shadow step

----            local pl = data.Player
----            if table.find(assassinClass, pl.Class) then
----                local sp = const.Spells
----                local spell = 0
----                if data and data.Object and data.Object.Spell then
----                    spell = data.Object.Spell
----                end
----                if (spell == sp.IceBolt) then
----                        --AddCombatLog("Is Shadow Step")

----                    local s, m = SplitSkill(pl:GetSkill(const.Skills.Water))

----                    local meleeRange = 328
----                    local range = meleeRange * 5
----                    if (m >= 4) then
----                        range = meleeRange * 10
----                    end
----                    local mon = t.Monster
----                    local dx = mon.X - Party.X
----                    local dy = mon.Y - Party.Y
----                    local dz = mon.Z - Party.Z
----                    local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
----                    local nx = dx / dist
----                    local ny = dy / dist
----                    local nz = dz / dist
----                    if dist > range then
----                        AddCombatLog("Target is out of range for Shadow Step")
----                        t.Result = 0
----                        return
----                    else
----                        Game.ShowStatusText("Shadow Step!")
----                        local monName="???"
----                        if(mon.NameId>0) then
----                            monName = Game.PlaceMonTxt[mon.NameId]
----                        else
----                            monName = Game.MonstersTxt[mon.Id].Name
----                        end
----                        AddCombatLog("Shadow Stepped to " .. monName)
----                        Party.X = mon.X - nx * meleeRange / 2
----                        Party.Y = mon.Y - ny * meleeRange / 2
----                        Party.Z = mon.Z
----                        Party.Direction = calculateDirection(Party.X, Party.Y, mon.X, mon.Y)
----                        -- damage already done in zzClasses?

----                        --                    local baseDamage = pl:GetMeleeDamageMin()
----                        --                    local maxDamage = pl:GetMeleeDamageMax()
----                        --                    local randomDamage = math.random(baseDamage, maxDamage) + math.random(baseDamage, maxDamage)
----                        --                    local damage = round(randomDamage / 2)
----                        --                    critChance, critMult, success = getCritInfo(pl, false, getMonsterLevel(t.Monster))
----                        --                    if success then
----                        --                        damage = damage * critMult
----                        --                        crit = true
----                        --                    end
----                        --                    for i = 0, 1 do
----                        --                        local it = pl:GetActiveItem(i)
----                        --                        if it then
----                        --                            local damage1 = calcFireAuraDamage(pl, it, 0, false, false, "damage")
----                        --                            local damage2 = calcEnchantDamage(pl, it, 0, false, false, "damage")
----                        --                            damage = damage + damage1 + damage2
----                        --                        end
----                        --                    end
----                        --                    local res = t.Monster.Resistances[t.DamageKind] or t.Monster.Resistances[4]
----                        --                    damage = damage / 2 ^(res % 1000 / 100)
----                        --                    local mult = damageMultiplier[t.PlayerIndex]["Melee"]
----                        --                    t.Result = damage * mult

----                        --                    if pl.Weak > 0 then
----                        --                        t.Result = t.Result * 0.5
----                        --                    end

----                        --                    if assassinSpells[spell].DamageMult then
----                        --                        t.Result = t.Result * assassinSpells[data.Object.Spell].DamageMult
----                        --                    end
----                    end


----                end
----            end
----        end
----    end
----end
