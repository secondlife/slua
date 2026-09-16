counter = 0

function LLEvents.timer()
    counter += 1
    preempt()
    counter += 1
end

function LLEvents.moving_start()
    print(if counter == 2 then "counter ok" else "counter bad")
end
