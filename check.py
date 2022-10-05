a = input()
with open("input.in") as input_file:
    with open(a) as output_file:
        b = input_file.read()
        c = output_file.read()

correct = 0
for i in range(10000):
    if b[i] == c[i]:
        correct += 1
    else:
        print(i)

print()
print(correct)