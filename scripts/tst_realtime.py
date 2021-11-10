from realtime_plot.drawer import create_realplot
import random
import numpy as np
import time






QUE = create_realplot(300, 35)


T0 = time.time()
while True:
	v1 = (random.random() + 1) * (time.time()- T0)
	v2 = (random.random() + 2) * (time.time()- T0)
	v3 = (random.random() + 3) * (time.time()- T0)
	n1 = "Anni"
	n2 = "Kalle"
	n3 = "Oscar"
	QUE.put( (n1,v1) )
	QUE.put( (n2,v2) )
	QUE.put( (n3,v3) )
	time.sleep(0.03)










