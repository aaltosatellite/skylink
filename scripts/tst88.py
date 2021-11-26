import random
import numpy as np



N = 100000
OK = []
for i in range(N):
	x = random.random() * 2 * np.pi * 1000
	y = np.sin(x)
	if abs(y) > 0.95:
		OK.append(x)

print("Ratio: {} / {}  = {}".format(len(OK), N, len(OK)/N))



