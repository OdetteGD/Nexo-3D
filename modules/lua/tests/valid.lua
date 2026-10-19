local total = 0

for i = 1, 5 do
    total = total + i
end

if total ~= 15 then
    error("unexpected total")
end

print("Lua syntax fixture passed")
