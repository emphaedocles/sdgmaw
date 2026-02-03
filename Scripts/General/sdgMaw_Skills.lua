
local MMPatch_UI = Game.PatchOptions.UILayoutActive()

local function InitSDGSettings()
    -- debug.Message("Init")

    SDGMAWSETTINGS = SDGMAWSETTINGS or { }

    SDGMAWSETTINGS.ftDamageToMonsters = true
    SDGMAWSETTINGS.ftDamageToPlayers = true
    SDGMAWSETTINGS.ftMonsterNameMinor = false
    SDGMAWSETTINGS.ftMonsterNameBoss = true
    SDGMAWSETTINGS.ftMonstrHealtherMinor = true
    SDGMAWSETTINGS.ftMonsterHealthBoss = true
    SDGMAWSETTINGS.ftPlayerHealing = true
    SDGMAWSETTINGS.ftAbbreviateNames = true
    SDGMAWSETTINGS.drgAdj=true
    -- debug.Message(SDGMAWSETTINGS)
end

-- new stealth skill
function events.GameInitialized2()

    InitSDGSettings()

    local stealthSkill = 54
    Skillz.new_armor(stealthSkill)
    Skillz.setName(stealthSkill, "Stealth")
    Skillz.setDesc(stealthSkill, 1, "Stealth/Finesse. Increase the chance for this character to be successfully Covered.  Has a chance to gain a Backstab stack when succsfully covered.\nBackstab stacks are used to enhance next attack \n Masteries learnt at 8-16-32")
    Skillz.setDesc(stealthSkill, 2, "Normal Cover bonus")
    Skillz.setDesc(stealthSkill, 3, "Double Cover bonus, chance to backstab per skill point. Backstab damage multiplier 1.5x")
    Skillz.setDesc(stealthSkill, 4, "Double Cover bonus, chance to backstab per skill point x2 Backstab damage multiplier 2x")
    Skillz.setDesc(stealthSkill, 5, "Triple Cover bonus, chance to backstab per skill point x3, Backstab damage multiplier 3x")
    Skillz.learn_at(stealthSkill, 30)

   InitCombatLog()
    
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

-- combat text stuff
function events.GameInitialized2()

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

    retalStacks = { }
    for i = 0, 4 do
        retalStacks[i] = CustomUI.CreateText {
            Text = "",
            Layer = 1,
            Screen = 0,
            X = 5 + i * 105,
            Y = 400,
            ColorStd = RGB(0,255,0),
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

    charDamTickStart = 25
    charHealTickStart = 100

    txtCharDam = { }
    for i = 0, 4 do
        txtCharDam[i] = { }
        txtCharDam[i]["txt"] = CustomUI.CreateText {
            Text = "",
            Layer = 1,
            Screen = 0,
            X = 25 + i * 110,
            -- 5+i*96, Y = 410
            Y = bsBaseY,
            ColorStd = RGB(255,0,0),
            Width = 250,
            Height = 20,
            Font = Game.Smallnum_fnt,
            AlignLeft = true,
        }

        txtCharDam[i]["tick"] = 0
        txtCharDam[i]["dam"] = 0

    end
    charDamClear = { }
    for i = 0, 4 do
        charDamClear[i] = false
    end

    -- damage to monsters
    txtMonDam = { }
    for i = 0, 4 do
        txtMonDam[i] = { }
        txtMonDam[i]["txt"] = CustomUI.CreateText {
            Text = "",
            Layer = 1,
            Screen = 0,
            X = 640 / 2,
            -- 5+i*96, Y = 410
            Y = 480 / 2,
            ColorStd = RGB(192,0,0),
            Width = 250,
            Height = 20,
            Font = Game.Smallnum_fnt,
            AlignLeft = true,
        }

        txtMonDam[i]["tick"] = 0
        txtMonDam[i]["dam"] = 0
    end
    txtMonDamCrit = { }
    for i = 0, 4 do
        txtMonDamCrit[i] = { }
        txtMonDamCrit[i]["txt"] = CustomUI.CreateText {
            Text = "",
            Layer = 1,
            Screen = 0,
            X = 640 / 2,
            -- 5+i*96, Y = 410
            Y = 480 / 2,
            ColorStd = RGB(192,192,32),
            Width = 250,
            Height = 20,
            Font = Game.Smallnum_fnt,
            AlignLeft = true,

        }

        txtMonDamCrit[i]["tick"] = 0
        txtMonDamCrit[i]["dam"] = 0
    end

    monDamClear = { }
    for i = 0, 4 do
        monDamClear[i] = false
    end
    monDamCritClear = { }
    for i = 0, 4 do
        monDamCritClear[i] = false
    end
    txtCharHealed = { }
    for i = 0, 4 do
        txtCharHealed[i] = { }
        txtCharHealed[i]["txt"] = CustomUI.CreateText {
            Text = "",
            Layer = 1,
            Screen = 0,
            X = 25 + i * 110,
            -- 5+i*96, Y = 410
            Y = bsBaseY,
            ColorStd = RGB(128,128,255),
            Width = 250,
            Height = 20,
            Font = Game.Smallnum_fnt,

            AlignLeft = true,
        }

        txtCharHealed[i]["tick"] = 0
        txtCharHealed[i]["heal"] = 0

    end

    charHealClear = { }
    for i = 0, 4 do
        charHealClear[i] = false
    end


end

function SetCharDamText(t)
    --   if (not SDGMAWSETTINGS.ftDamageToPlayers) then return end

    local pl = t.Player
    local dam = t.Result
    if (dam > 0) then
        txtCharDam = txtCharDam or { }

        local idx = pl:GetIndex()
        for i = 0, Party.High do
            local pidx = Party[i]:GetIndex()
            if (pidx == idx) then
                txtCharDam[i] = txtCharDam[i] or { }
                local d = txtCharDam[i]["dam"] or 0

                if (d > 0) then
                    d = d + dam
                    local txt = shortenNumber(d, 4, true)
                    local ti = txtCharDam[i]["tick"]
                    if (ti < 10) then
                        ti = ti + 10
                    end
                    txtCharDam[i]["txt"].Text = txt
                    txtCharDam[i]["tick"] = ti
                    txtCharDam[i]["dam"] =(d)
                else
                    local txt = shortenNumber(dam, 4, true)
                    txtCharDam[i]["tick"] = charHealTickStart
                    txtCharDam[i]["dam"] = dam
                    txtCharDam[i]["txt"].Text = txt
                end


            end

        end


    end
end

function SetHealText(playerNdx, heal)
    -- if (not SDGMAWSETTINGS.ftPlayerHealing) then return end

    if (heal > 0) then
        txtCharHealed = txtCharHealed or { }
        local pidx = playerNdx
        -- Party[i]:GetIndex()
        txtCharHealed[pidx] = txtCharHealed[pidx] or { }
        local d = txtCharHealed[pidx]["heal"] or 0

        if (d > 0) then
            d = d + heal
            local ti = txtCharHealed[pidx]["tick"]
            local txt = shortenNumber(d, 4, true)
            if (ti < 10) then
                ti = ti + 10
            end
            txtCharHealed[pidx]["txt"].Text = txt
            txtCharHealed[pidx]["txt"].ColorStd = RGB(128, 128, 255)

            txtCharHealed[pidx]["tick"] = ti
            txtCharHealed[pidx]["heal"] =(d)
        else
            local txt = shortenNumber(heal, 4, true)
            txtCharHealed[pidx]["tick"] = charHealTickStart
            txtCharHealed[pidx]["heal"] = heal
            txtCharHealed[pidx]["txt"].Text = txt
        end
    end

end
function SetLeechText(playerNdx, heal)
    -- if (not SDGMAWSETTINGS.ftPlayerHealing) then return end
    if (heal > 0) then
        txtCharHealed = txtCharHealed or { }

        local pidx = 0
        -- Party[partyNdx]:GetIndex()
        for i = 0, Party.High do
            if Party[i]:GetIndex() == playerNdx then
                pidx = i
            end
        end

        txtCharHealed[pidx] = txtCharHealed[pidx] or { }
        local d = txtCharHealed[pidx]["heal"] or 0

        if (d > 0) then
            d = d + heal
            local txt = shortenNumber(d, 4, true)
            local ti = txtCharHealed[pidx]["tick"]
            if (ti < 10) then
                ti = ti + 10
            end
            txtCharHealed[pidx]["txt"].Text = txt
            txtCharHealed[pidx]["txt"].ColorStd = RGB(255, 0, 255)

            txtCharHealed[pidx]["tick"] = ti
            txtCharHealed[pidx]["heal"] =(d)
        else
            local txt = shortenNumber(heal, 4, true)
            txtCharHealed[pidx]["tick"] = charHealTickStart
            txtCharHealed[pidx]["heal"] = heal
            txtCharHealed[pidx]["txt"].Text = txt
        end
    end

end



function events.Tick()
    -- SDGMAWSETTINGS=SDGMAWSETTINGS or {}

    -- if (SDGMAWSETTINGS.ftPLayerHealing) then
    txtCharDam = txtCharDam or { }
    for i = 0, Party.High do
        local pl = Party[i]
        local id = pl:GetIndex()
        -- char healed text
        txtCharHealed[i] = txtCharHealed[i] or { }
        txtCharHealed[i]["tick"] = txtCharHealed[i]["tick"] or 0
        if (txtCharHealed[i]["tick"] > 0) then
            txtCharHealed[i]["txt"].Y = bsBaseY -(charHealTickStart - txtCharHealed[i]["tick"]);
            txtCharHealed[i]["tick"] = txtCharHealed[i]["tick"] -1
            Game.NeedRedraw = true
            charHealClear[i] = true
        else
            if (charHealClear[i]) then
                txtCharHealed[i]["txt"].Text = ""
                txtCharHealed[i]["heal"] = 0
                charHealClear[i] = false
                Game.NeedRedraw = true
            end
        end
        -- end

        --        if (SDGMAWSETTINGS.ftDamageToPlayers) then
        -- char damaged text
        txtCharDam[i] = txtCharDam[i] or { }
        txtCharDam[i]["tick"] = txtCharDam[i]["tick"] or 0
        if (txtCharDam[i]["tick"] > 0) then
            txtCharDam[i]["txt"].Y = bsBaseY -(charDamTickStart - txtCharDam[i]["tick"]);
            txtCharDam[i]["tick"] = txtCharDam[i]["tick"] -1
            Game.NeedRedraw = true
            charDamClear[i] = true
        else
            if (charDamClear[i]) then
                txtCharDam[i]["txt"].Text = ""
                txtCharDam[i]["dam"] = 0
                charDamClear[i] = false
                Game.NeedRedraw = true
            end
        end
        -- end

        -- if (SDGMAWSETTINGS.ftDamageToMonsters) then
        -- mon damaged text
        txtMonDam[i] = txtMonDam[i] or { }
        txtMonDam[i]["tick"] = txtMonDam[i]["tick"] or 0
        if (txtMonDam[i]["tick"] > 0 and txtMonDam[i]["txt"].Y > 0 and txtMonDam[i]["txt"].X > 0) then
            txtMonDam[i]["txt"].Y = txtMonDam[i]["txt"].Y - 1
            txtMonDam[i]["tick"] = txtMonDam[i]["tick"] -1
            Game.NeedRedraw = true
            monDamClear[i] = true
        else
            if (monDamClear[i]) then
                txtMonDam[i]["txt"].Text = ""
                txtMonDam[i]["dam"] = 0
                monDamClear[i] = false
                Game.NeedRedraw = true
            end
        end
        txtMonDamCrit[i] = txtMonDamCrit[i] or { }
        txtMonDamCrit[i]["tick"] = txtMonDamCrit[i]["tick"] or 0
        if (txtMonDamCrit[i]["tick"] > 0 and txtMonDamCrit[i]["txt"].Y > 0 and txtMonDamCrit[i]["txt"].X > 0) then
            txtMonDamCrit[i]["txt"].Y = txtMonDamCrit[i]["txt"].Y - 1
            txtMonDamCrit[i]["tick"] = txtMonDamCrit[i]["tick"] -1
            Game.NeedRedraw = true
            monDamCritClear[i] = true
        else
            if (monDamCritClear[i]) then
                txtMonDamCrit[i]["txt"].Text = ""
                txtMonDamCrit[i]["dam"] = 0
                monDamCritClear[i] = false
                Game.NeedRedraw = true
            end
        end
        -- end

        -- char backstab stacks
        local s, m = SplitSkill(Skillz.get(pl, 54))
        if s > 0 and m > 1 then
            vars.backstab = vars.backstab or { }
            if (vars.backstab[id]) then
                vars.backstab[id]["Stacks"] = vars.backstab[id]["Stacks"] or 0
                vars.backstab[id]["Damage"] = vars.backstab[id]["Damage"] or 0
                vars.backstab[id].Time = vars.backstab[id].Time or 0

                if (vars.backstab[id]["Stacks"] > 0) then
                    local strStacks = ""
                    for j = 1, vars.backstab[id]["Stacks"] do
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

                if (bsDamTick[id] > 0 and vars.backstab[id]["Damage"] > 0) then
                    local strDamage = string.format(math.floor(vars.backstab[id]["Damage"]))
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

        vars.retaliation = vars.retaliation or { }
        if (vars.retaliation[id]) then
            vars.retaliation[id]["Stacks"] = vars.retaliation[id]["Stacks"] or 0
            if (vars.retaliation[id]["Stacks"] > 0) then
                local strStacks = ""
                for j = 1, vars.retaliation[id]["Stacks"] do
                    strStacks = strStacks .. "+"
                end
                retalStacks[i].Text = StrColor(0, 255, 255, strStacks)
            else
                retalStacks[i].Text = ""
            end
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
    --local screenX = screenCenterX - math.max(math.min(horizontal_offset * scale_x, screenCenterX ), - screenCenterX )
    --local screenY = screenCenterY - math.max(math.min(vertical_offset * 2 * scale_y + height *(screenHeight / distance) ^ 0.5, screenCenterY ), - screenCenterY )

    return screenX, screenY
end

function AddDamageToMonText(mon, dam, critx, player, backstabx, backstabmult)
    --  if (not SDGMAWSETTINGS.ftDamageToMonsters) then return end

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



-- monster health/name display
-- ************************************************************

function events.LoadMap()
    -- mapvars.damaged_monsters = mapvars.damaged_monsters or { }

    -- monster health display
    txtMonHP = txtMonHP or { }

end
function events.AfterLoadMap()
    -- SDGMAWSETTINGS= SDGMAWSETTINGS or {}

    if not Map or not Map.Monsters or type(Map.Monsters.High) ~= "number" then return end

    -- create boss monster Tags right away
    for i = 0, Map.Monsters.High do
        mon = Map.Monsters[i]
        if mon then
            -- and mon.NameId >= 220 and mon.NameId < 300 then
            --if (mon.HP > 0 and(mon.AIState ~= 5 and mon.AIState ~= 11)) then
                --                mapvars.damaged_monsters[i] = mapvars.damaged_monsters[i] or { }
                --                mapvars.damaged_monsters[i].timestamp = os.clock()
                --                mapvars.damaged_monsters[i].hp_before_hit = mon.HP

                local name = Game.MonstersTxt[mon.Id].Name
                if mon.NameId > 0 then
                    name = Game.PlaceMonTxt[mon.NameId]
                end
                --  name= name .. " " .. mon.AIState
                txtMonHP[i] = txtMonHP[i]
                if (not txtMonHP[i]) then
                    txtMonHP[i] = CustomUI.CreateText {
                        Text = name,
                        Layer = 2,
                        Screen = 0,
                        X = 640 / 2,
                        -- 5+i*96, Y = 410
                        Y = 480 / 2,
                        ColorStd = RGB(64,192,64),
                        Width = 250,
                        Height = 20,
                        AlignLeft = true,
                        Font = Game.Smallnum_fnt,
                        Active = false-- deactive until in range etc...
                    }
                    
                end
          --  end
        end
    end
       -- AddCombatLog("Monsters on map " .. Map.Monsters.High)
    

--    if(not txtMonOnMap) then
--      txtMonOnMap=CustomUI.CreateText {
--                        Text = name,
--                        Layer = 2,
--                        Screen = 0,
--                        X = 540,
--                        -- 5+i*96, Y = 410
--                        Y = 0,
--                        ColorStd = RGB(64,192,192),
--                        Width = 100,
--                        Height = 100,
--                        AlignLeft = true,
--                        Font = Game.Smallnum_fnt,
--                        Active = true
--                    }
--    end
--    txtMonOnMap.Text="Mobs " .. Map.Monsters.High
    

end



-- On every tick, decide which monsters to show and write to JSON
function events.Tick()
    -- SDGMAWSETTINGS=SDGMAWSETTINGS or {}

    if Game.CurrentScreen ~= 0 then return end
    -- Only process in game screen
    local bRedraw = false

    -- get maxID Monster
    local maxS = 0
    local maxM = 0

    for i = 0, Party.High do
        local s, m = SplitSkill(Party[i]:GetSkill(const.Skills.IdentifyMonster))
        local s1 = SplitSkill(Party[i].Skills[const.Skills.IdentifyMonster])
        if s1 > 0 then
            if s * m > maxS then
                maxS = s * m
            end
            if m > maxM then
                maxM = m
            end
        end
    end
    local distAdj=1+ maxM/4

    local alive=0
    local total=Map.Monsters.High+1
    local inRange=0
    local onMap=0
    local screenSpace=0
    local activeC=0
    


    for i = 0, Map.Monsters.High do
        local mon = Map.Monsters[i]

        local exponent = math.floor(mon.Resistances[0] / 1000)
        local hp = mon.HP * 2 ^ exponent
        local basehp=mon.HP
        local fullHP = mon.FullHP * 2 ^ exponent
        local bLifeDetect =maxS>4 or Party.SpellBuffs[const.PartyBuff.DetectLife].ExpireTime > Game.Time or hp < fullHP

        if(baseHP>0 and not(mon.AIState == 5 or mon.AIState == 11 or mon.AIState == 19)) then
         alive=alive +1
        end

        if bLifeDetect and mon.ShowOnMap and mon.ShowAsHostile and baseHP > 0 and not(mon.AIState == 5 or mon.AIState == 11 or mon.AIState == 19) then
           onMap=onMap+1

            local dist = getDistance(Party, mon)

            local barSize = CheckBarSize(mon)
            local bTier = barSize>=250 or hp<fullHP


            if dist <= (2000*distAdj) and bTier then
                inRange=inRange+1

                local name = Game.MonstersTxt[mon.Id].Name
                if mon.NameId > 0 then
                    name = Game.PlaceMonTxt[mon.NameId]
                end
                -- name= name .. " " .. mon.AIState
                --                if(SDGMAWSETTINGS.ftAbbreviateNames) then
                --                    name=first_chars_of_words(name)
                --                end

                local height = Game.MonListBin[mon.Id].Height
                local hAdj = 1

                if (barSize >= 250) then hAdj = 1.5 end
                if (dist < 1000) then
                    hAdj = 0.75
                end

                local screenX, screenY = ProjectToScreen(mon.X, mon.Y, mon.Z, height * hAdj)

                if screenX and screenY then
                    screenSpace=screenSpace+1
                    if (screenY < 50) then screenY = 50 end
                    if(not txtMonHP[i]) then
                        txtMonHP[i] = CustomUI.CreateText {
                        Text = name,
                        Layer = 2,
                        Screen = 0,
                        X = 640 / 2,
                        -- 5+i*96, Y = 410
                        Y = 480 / 2,
                        ColorStd = RGB(64,192,64),
                        Width = 250,
                        Height = 20,
                        AlignLeft = true,
                        Font = Game.Smallnum_fnt,
                        Active = true-- deactive until in range etc...
                    }
                    
                    end
                    txtMonHP[i] = txtMonHP[i] or { }

                    local cr, cg, cb
                    cr = 0
                    cg = 148
                    cb = 0
                    if (barSize >= 250) then
                        -- is boss
                        txtMonHP[i].Font = Game.Lucida_fnt
                        if (barSize >= 400) then
                            -- omni
                            cr = 255
                            cg = 192
                            cb = 64
                        elseif (barSize >= 300) then
                            -- brood
                            cr = 255
                            cg = 128
                            cb = 32
                        else
                            cr = 255
                            cg = 64
                            cb = 16
                        end
                    else
                        txtMonHP[i].Font = Game.Smallnum_fnt
                        cr = 0
                        cg = 192
                        cb = 0
                        name = ""
                        -- show health only on minions
                    end
                    local status=""
                    if(mon.AIState==8) then
                    --stunned
                     status="<Stunned>"
                    end
                    if(mon.AIState==15) then
                    --paralyzed
                    status="<Paralyzed>"
                    end
                    local txt = string.format("%s  %s/%s ", name, hp, fullHP, status)

                    txtMonHP[i].Text = StrColor(cr, cg, cb, txt)
                    txtMonHP[i].X = screenX
                    txtMonHP[i].Y = screenY
                    txtMonHP[i].Active = true
                    activeC=activeC+1

                    bRedraw = true
                else
                    -- offscreen
                    txtMonHP[i] = txtMonHP[i] or { }
                    if (txtMonHP[i].Active) then bRedraw = true end
                  txtMonHP[i].Active = false
                end
            else
                -- too far away or filtered by tier
                txtMonHP[i] = txtMonHP[i] or { }
                if (txtMonHP[i].Active) then bRedraw = true end
                txtMonHP[i].Active = false
            end
        else
            -- not acvie, not hostile etc
            txtMonHP[i] = txtMonHP[i] or { }
            if (txtMonHP[i].Active) then bRedraw = true end
            txtMonHP[i].Active = false
        end

    end
    
   

    if (bRedraw) then Game.NeedRedraw = true end

end

-- Function to get the first character of each word in a string
function first_chars_of_words(input)
    local out = ""
    -- Validate input type
    if type(input) ~= "string" then
        -- error("Input must be a string")
        return out
    end

    -- %S+ matches sequences of non-space characters (words)
    for word in input:gmatch("%S+") do
        -- Get the first UTF-8 character (works for ASCII too)
        local first_char = word:sub(1, 1)
        -- table.insert(result, first_char)
        out = out .. " " .. first_char
    end

    return out
end


local barSize = { 100, 150, 200, 250, 300, 400 }
function CheckBarSize(mon)
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
    return barSize[monType]
end

function InitCombatLog()
    --InitSDGOverlayLog()
    iCombatLogRows=8
    iLastCombatLog=0--will clear on new map
    iCombatLogBaseY=75
    txtCombatLog={}
    ShowCombatLog=true

--    local TogglePreview = CustomUI.CreateButton{
--	IconUp = "tab6a",
--	IconDown = "tab6b",
--	Masked = true,
--	Screen = {0, const.Screens.SelectTarget},
--	Layer = 0,
--	X = MMPatch_UI and 477 or -1,
--	Y = MMPatch_UI and 30 or 100,
--	Condition = InGame,
--	MouseOverAction = function()
--		Game.ShowStatusText(ShowCombatLog and "Hide CLog" or "Show CLog")
--	end,
--	Action = function(t)
--		ShowCombatLog = not ShowCombatLog
--		t.IUpSrc, t.IDwSrc = t.IDwSrc, t.IUpSrc
--		t.IUpPtr, t.IDwPtr = t.IDwPtr, t.IUpPtr
--		Game.NeedRedraw = true
--	end,
--	Key = "ShowCombatLog"
--}

     for i=0,iCombatLogRows-1 do     
        txtCombatLog[i]=CustomUI.CreateText {
                        Text ="",
                        Layer = 3,
                        Screen = {0,20},
                        X = 640-300,
                        -- 5+i*96, Y = 410
                        Y = iCombatLogBaseY+ (i*28),
                        ColorStd = RGB(128,192,192),
                        Width = 300,
                        Height = 28,
                        AlignLeft = true,
                        Font = Game.Smallnum_fnt,
                        Active = false
                    }
        
    end
end
function events.KeyUp(t)
    if(Game.CurrentScreen ~=0) then return end

    if(t.Key==88)then
    --L key
     if(ShowCombatLog) then
       ShowCombatLog=false
     else
        ShowCombatLog=true
     end
      for i=0,iLastCombatLog-1 do  
        txtCombatLog[i].Active=ShowCombatLog
      end
      Game.Redraw=true
    end
    if(t.Key==86) then
        ClearCombatLog()
    end
end
function events.LeaveMap()
 --ClearCombatLog()
  
end
function events.ExitMapAction(t)
--like exit game, death exit etc
 --ClearCombatLog()-- don't clear on death
 
end
function AddCombatLog(msg)
  local iRow=iLastCombatLog
  if(iRow >= iCombatLogRows) then
    iRow=iCombatLogRows-1
    --move all items up one level
    for i=0,iCombatLogRows-2 do
        txtCombatLog[i].Text=txtCombatLog[i+1].Text
    end
  end
    txtCombatLog[iRow].Text=msg
    txtCombatLog[iRow].Active=ShowCombatLog
    Game.Redraw=true

  iLastCombatLog=iRow+1
end
function ClearCombatLog()
  iLastCombatLog=0
  for i=0,iCombatLogRows-1 do
    txtCombatLog[i].Text=""
    txtCombatLog[i].Active=false
  end
  Game.Redraw=true
end
