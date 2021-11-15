from realtime_plot.drawer import create_realplot
import random
import numpy as np
import time
import os
import sys

os.startfile("/home/elmore/media/audio/Into the West.mp3")
sys.exit(1)




QUE1 = create_realplot(10, 38)


T0 = time.time()
while True:
	v1 = (2*np.sin(time.time()*0.2+1) + random.gauss(0,0.2)) * np.sin(time.time()*0.07) * 0.1
	v2 = (3*np.sin(time.time()*0.6+2) + random.gauss(0,0.2)) * np.sin(time.time()*0.07) * 0.1
	v3 = (4*np.sin(time.time()*0.9+3) + random.gauss(0,0.3)) * np.sin(time.time()*0.07) * 0.1
	#v4 = 4*np.sin(time.time()*2) * np.sin(time.time()*0.8) * 0.6
	n1 = "Anni"
	n2 = "Kalle"
	n3 = "Oscar"
	#n4 = "Ida"
	QUE1.put( (n1,v1) )
	QUE1.put( (n2,v2) )
	#QUE.put( (n3,v3) )
	#QUE.put( (n4,v4) )
	time.sleep(0.06)










