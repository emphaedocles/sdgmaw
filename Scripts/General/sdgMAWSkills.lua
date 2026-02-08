-- stealth skill for MAW
-- new stealth skill
function events.GameInitialized2()

    g_allowStealth = true
    -- if not present or false, Stealth will be ignored even if taken


    local stealthSkill = 54
    Skillz.new_armor(stealthSkill)
    Skillz.setName(stealthSkill, "Stealth")
    Skillz.setDesc(stealthSkill, 1, "Stealth/Finesse. Increase the chance for this character to be successfully Covered.  Has a chance to gain a Backstab stack when succsfully covered.\nBackstab stacks are used to enhance next attack \n Masteries learnt at 8-16-32")
    Skillz.setDesc(stealthSkill, 2, "Normal Cover bonus")
    Skillz.setDesc(stealthSkill, 3, "Double Cover bonus, chance to backstab per skill point. Backstab damage multiplier 1.5x")
    Skillz.setDesc(stealthSkill, 4, "Double Cover bonus, chance to backstab per skill point x2 Backstab damage multiplier 2x")
    Skillz.setDesc(stealthSkill, 5, "Triple Cover bonus, chance to backstab per skill point x3, Backstab damage multiplier 3x")
    Skillz.learn_at(stealthSkill, 30)



    backstabStacks = { }
    for i = 0, 4 do
        backstabStacks[i] = CustomUI.CreateText {
            Text = "",
            Layer = 1,
            Screen = 0,
            X = 5 + i * 105,
            Y = 390,
            ColorStd = RGB(255,0,0),
            Width = 50,
            Height = 20,
            Font = Game.Smallnum_fnt,
        }
    end

    bsBaseY = 400
    bsDamTicksStart = 100
    backstabDamageDone = { }
    for i = 0, 4 do
        backstabDamageDone[i] = CustomUI.CreateText {
            Text = "",
            Layer = 1,
            Screen = 0,
            X = 5 + i * 105,
            -- 5+i*96, Y = 410
            Y = bsBaseY,
            ColorStd = RGB(255,255,0),
            Width = 250,
            Height = 20,
            Font = Game.Smallnum_fnt,
        }


    end
    bsDamTick = { }
    for i = 0, 4 do
        bsDamTick[i] = 0
    end
    bsDamTickClear = { }
    for i = 0, 4 do
        bsDamTickClear[i] = false
    end


end


-- Stealth skill, points add
function events.Action(t)
    if t.Action == 121 then
        if t.Param == 54 then
            local stealthRequirements = { 8, 16, 32 }
            local pl = Party[Game.CurrentPlayer]
            local s, m = SplitSkill(Skillz.get(pl, 54))
            if s == 32 then
                t.Handled = true
                Game.ShowStatusText("This skill has reached its limit")
            elseif s > 32 then
                t.Handled = true
                while s > 32 do
                    pl.SkillPoints = pl.SkillPoints + s
                    s = s - 1
                end
                Skillz.set(pl, 54, JoinSkill(s, m))
            end
            if pl.SkillPoints > s and stealthRequirements[m] and s + 1 >= stealthRequirements[m] and Skillz.MasteryLimit(pl, 54) > m then
                Skillz.set(pl, 54, JoinSkill(s, m + 1))
            elseif stealthRequirements[m] and s >= stealthRequirements[m] and Skillz.MasteryLimit(pl, 54) > m then
                Skillz.set(pl, 54, JoinSkill(s, m + 1))
            end
        end
    end
end

function events.Tick()

    -- char backstab stacks

    for i = 0, Party.High do
        local pl = Party[i]
        local id = pl:GetIndex()

        local s, m = SplitSkill(Skillz.get(pl, 54))
        if s > 0 then
            g_backstab = g_backstab or { }
            if (g_backstab[id]) then
                g_backstab[id]["Stacks"] = g_backstab[id]["Stacks"] or 0
                g_backstab[id]["Damage"] = g_backstab[id]["Damage"] or 0

                if (g_backstab[id]["Stacks"] > 0) then
                    local strStacks = ""
                    for j = 1, g_backstab[id]["Stacks"] do
                        strStacks = strStacks .. "+"
                    end
                    backstabStacks[i].Text = StrColor(255, 0, 0, strStacks)
                else
                    backstabStacks[i].Text = ""
                end
                bsDamTick = bsDamTick or { }
                bsDamTick[id] = bsDamTick[id] or 0
                bsDamTickClear = bsDamTickClear or { }
                bsDamTickClear[id] = bsDamTickClear[id] or false

                if (bsDamTick[id] > 0 and g_backstab[id]["Damage"] > 0) then
                    local strDamage = string.format(math.floor(g_backstab[id]["Damage"]))
                    backstabDamageDone[i].Text = strDamage
                    backstabDamageDone[i].Y = bsBaseY -(bsDamTicksStart - bsDamTick[id]);
                    Game.NeedRedraw = true
                    bsDamTick[id] = bsDamTick[id] -1
                    bsDamTickClear[id] = true
                else
                    backstabDamageDone[i].Text = ""
                    if bsDamTickClear[id] then
                        Game.NeedRedraw = true
                        bsDamTickClear[id] = false
                    end
                end
            end
        else
                backstabStacks[i].Text = ""
        end
    end
end
-- Distance utility
local function getDistance(p, m)
    return math.sqrt((p.X - m.X) ^ 2 +(p.Y - m.Y) ^ 2 +(p.Z - m.Z) ^ 2)
end

local screenWidth = 640.0-- MM Engine internal screen size (UILayout Off)
local screenHeight = 480.0
local screenCenterX = screenWidth / 2.0
local screenCenterY = screenHeight / 2.0

local function ProjectToScreen(x, y, z, height)
    local dx, dy, dz = x - Party.X, y - Party.Y, z - Party.Z

    local angle = math.rad(Party.Direction * 360.0 / 2048.0)
    local look = math.rad(Party.LookAngle * 180.0 / 1024.0)
    local s = math.sin(angle)
    local c = math.cos(angle)

    local depth = dx * c + dy * s
    local horizontal_offset = - dx * s + dy * c

    -- If depth is zero or negative, the object is behind the camera and should not be drawn.
    if depth <= 0.1 then
        -- Use a small threshold to avoid division by zero
        return nil, nil
    end

    local distance = math.sqrt((Party.X - x) ^ 2 +(Party.Y - y) ^ 2 +(Party.Z - z) ^ 2)
    local vertical_offset = dz - depth * math.tan(look)
    local scale_x = screenCenterX / depth
    local scale_y = screenCenterY / depth
    local screenX = screenCenterX - math.max(math.min(horizontal_offset * 1.25 * scale_x, screenCenterX / 1.1), - screenCenterX / 1.1)
    local screenY = screenCenterY - math.max(math.min(vertical_offset * 2 * scale_y + height *(screenHeight / distance) ^ 0.5, screenCenterY / 1.1), - screenCenterY / 1.8)
    -- local screenX = screenCenterX - math.max(math.min(horizontal_offset * scale_x, screenCenterX ), - screenCenterX )
    -- local screenY = screenCenterY - math.max(math.min(vertical_offset * 2 * scale_y + height *(screenHeight / distance) ^ 0.5, screenCenterY ), - screenCenterY )

    return screenX, screenY
end
function AddDamageToMonTextBS(mon, dam, critx, player, backstabx, backstabmult)

    local height = Game.MonListBin[mon.Id].Height / 2
    local screenX, screenY = ProjectToScreen(mon.X, mon.Y, mon.Z, height)

    if (not screenX or screenX < 0 or screenX > screenWidth) then screenX = screenCenterX end
    if (not screenY or screenY < 26 or screenY > screenHeight) then screenY = screenCenterY end

    if (dam > 0) then

        local damTxt = shortenNumber(dam, 4, true)
        local txt = damTxt

        if (dam > mon.HP) then
            if (critx) then
                txt = damTxt .. "(KO!)"
            else
                txt = damTxt .. "(KO)"
            end
        elseif (critx) then
            txt = damTxt .. "(!)"
        end
        if (backstabx) then
            txt = string.format("%s %s%s)", txt, "(X", backstabmult)
            AddCombatLog(StrColor(255, 255, 0, string.format("%s hits %s for %s damage with Backstab x%s!", player.Name, Game.PlaceMonTxt[mon.NameId], damTxt, backstabmult)))
        end
        txtMonDam = txtMonDam or { }

        local idx = pl:GetIndex()
        for i = 0, Party.High do
            local pidx = Party[i]:GetIndex()
            if (pidx == idx) then

                local txtM = txt
                if (not critx and not backstabx) then
                    txtMonDam[i] = txtMonDam[i] or { }
                    txtMonDam[i]["tick"] = 25
                    txtMonDam[i]["dam"] = dam
                    txtMonDam[i]["txt"].Text = txtM
                    txtMonDam[i]["txt"].X = screenX
                    txtMonDam[i]["txt"].Y = screenY
                else
                    txtMonDamCrit[i] = txtMonDamCrit[i] or { }
                    txtMonDamCrit[i]["tick"] = 25
                    txtMonDamCrit[i]["dam"] = dam
                    txtMonDamCrit[i]["txt"].X = screenX
                    txtMonDamCrit[i]["txt"].Y = screenY
                    if (backstabx) then
                        txtMonDamCrit[i]["txt"].Text = StrColor(255, 128, 192, txtM)
                    else
                        txtMonDamCrit[i]["txt"].Text = txtM
                    end

                end
            end
        end
    end
end

function AddBackstabStack(player)
    -- backstab/counterstrick from steath skilled trigged by covered player on their next attack
    local st, mt = SplitSkill(Skillz.get(player, 54))
     if (st *(mt - 1)) / 100 > math.random() then
    
         local idxT = player:GetIndex()
        -- debugging , make it 100% chance to gain stack
        g_backstab = g_backstab or { }
        g_backstab[idxT] = g_backstab[idxT] or { }
        g_backstab[idxT]["Stacks"] = g_backstab[idxT]["Stacks"] or 0
        g_backstab[idxT]["Damage"] = g_backstab[idxT]["Damage"] or 0
        g_backstab[idxT]["Stacks"] = g_backstab[idxT]["Stacks"] + 1

        AddCombatLog(player.Name .. " gains a Backstab stack from Stealth! Total stacks: " .. g_backstab[idxT]["Stacks"])

    end
end

function GetStealthCoverBonus(player)
    local stealthCvrBonus = 0
    -- stealth skill will increase chance attacked character is covered instead
    local st, mt = SplitSkill(Skillz.get(player, 54))
    local stealthCvrBonus = 0
    if st > 0 then
        stealthCvrBonus = st * mt * 0.01
    end
    return stealthCvrBonus
end

function StealthBackstabDamage(t, res)
    backstabHit = false
    backstabMX = 0
    local id=t.Player:GetIndex()

    -- backstab code
    if g_backstab and g_backstab[id] then
        local pl = t.Player
        local s, m = SplitSkill(Skillz.get(pl, 54))
        local stacks = g_backstab[id].Stacks

        if (stacks > 0) then
            if (m < 2) then
                stacks = 1
            end

            local damMult = 1
            if m == 2 then
                damMult = 1.5
            elseif m == 3 then
                damMult = 2
            elseif m == 4 then
                damMult = 3
            end

            local totalBackstabDamage = 0
            -- (damage* (m-1))
            if damMult > 0 then
                t.Result = t.Result * damMult
                -- t.Result+totalRetDamage
                totalBackstabDamage = t.Result
                bsDamTick = bsDamTick or { }
                bsDamTick[id] = bsDamTick[id] or 0
                bsDamTick[id] = bsDamTicksStart or 100
                local r = 2 ^(res / 100)
                -- adjust for res
                totalBackstabDamage = totalBackstabDamage / r
                backstabHit = true
                backstabMX = damMult
            end

            g_backstab[id].Damage = totalBackstabDamage

            mapvars.backstabTrack = mapvars.backstabTrack or { }
            mapvars.backstabTrackCount = mapvars.backstabTrackCount or { }
            vars.backstabTrack = vars.backstabTrack or { }
            vars.backstabTrackCount = vars.backstabTrackCount or { }

            vars.backstabTrack[id] = vars.backstabTrack[id] or 0
            vars.backstabTrack[id] = vars.backstabTrack[id] + totalBackstabDamage
            mapvars.backstabTrack[id] = mapvars.backstabTrack[id] or 0
            mapvars.backstabTrack[id] = mapvars.backstabTrack[id] + totalBackstabDamage

            vars.backstabTrackCount[id] = vars.backstabTrackCount[id] or 0
            vars.backstabTrackCount[id] = vars.backstabTrackCount[id] + 1
            mapvars.backstabTrackCount[id] = mapvars.backstabTrackCount[id] or 0
            mapvars.backstabTrackCount[id] = mapvars.backstabTrackCount[id] + 1

            g_backstab[id].Stacks = math.max(stacks - 1, 0)

        else
            g_backstab[id].Damage = 0
        end

    end
    return t.Result
end