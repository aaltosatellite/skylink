import random
rint = random.randint




for i in range(1000000):
	m = rint(3,200)
	a = rint(-500,1000)
	b = rint(-500,1000)
	x1 = ((a%m) + (b%m)) % m
	x2 = (a+b)%m
	#print(x1, x2)
	assert(x1 == x2)








