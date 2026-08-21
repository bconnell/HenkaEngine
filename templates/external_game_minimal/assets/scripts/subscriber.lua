function OnEvent(event_id, source_entity)
    local action_down = Input.IsActionDown(7)
    local interaction = Interaction.Try(source_entity)
    local impulse_result = Physics.ApplyImpulse(source_entity, { x = 0.25, y = 0.0, z = 0.0 })
    if action_down and interaction == 3 and impulse_result == 0 then
        State.SetI32(77, event_id)
    end
end
