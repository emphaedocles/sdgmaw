local sdgMAWDLL =require("mylibtest")--  mem.dll[DevPath .. "Scripts/DLL/sdgMawOverlayCX.dll"]
----local sdgMAWDLL = package.loadlib("D:/games/MnMMaw4/mm8_quckstart/Scripts/DLL/sdgmawoverlaycx.dll", "luaopen_sdgmawoverlaycx")
----local sdgMAWDLL= mem.dll[DevPath .. "Scripts/DLL/sdgmawoverlaycx.dll"]

function InitSDGOverlayLog()
if (sdgMAWDLL) then
----debug.Message(sdgMAWDLL)
    sdgMAWDLL.showmsg("Log started..","SDG- MAW Overlay")

--sdgMAWDLL.showmsg("Tick", "for tock")

----for index, value in ipairs(sdgMAWDLL) do
----  debug.Message(index .. ": " .. value)
----end
--        --sdgMAWDLL.addtext("hello world")
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