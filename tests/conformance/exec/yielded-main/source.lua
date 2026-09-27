local total = 0
for i = 1, 100000 do
    total += i
end
done = total

function LLEvents.moving_start()
    print(if done == 5000050000 then "done ok" else "done bad")
end
