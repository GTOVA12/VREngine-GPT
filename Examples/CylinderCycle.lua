-- Run before each fixed simulation tick. Component 1 is the example cylinder.
local extending = true

function OnUpdate(deltaTime, time)
    if FactoryCore.GetSignal(1, "Fault") then
        FactoryCore.SetInput(1, "Extend", false)
        FactoryCore.SetInput(1, "Retract", false)
        return
    end
    if extending and FactoryCore.GetSignal(1, "Extended") then
        extending = false
    elseif not extending and FactoryCore.GetSignal(1, "Retracted") then
        extending = true
    end
    FactoryCore.SetInput(1, "Extend", extending)
    FactoryCore.SetInput(1, "Retract", not extending)
end
