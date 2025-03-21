N = int(input("Number of terms and nterms in grammar: "))
Smax = int(input("STACK_MAX: "))
T1 = float(input("Time for compilation with STACK_MAX=1 (sec): "))
T4 = float(input("Time for compilation with STACK_MAX=4 (sec): "))

total = 0.0

T_iter = (T4-T1)/3

for i in range(Smax):
    total += N**(i+1) * T_iter

print("Estimated total_time:", total)
