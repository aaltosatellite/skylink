from realtime_plot.drawer import create_realplot





if __name__ == '__main__':
	import numpy as np
	import time
	que = create_realplot(8000, 30)
	while True:
		a = np.sin(time.time() * 0.5)*2 + np.random.normal(0,0.2)
		b = np.sin(2+ time.time() * 1.5)*2 + np.random.normal(0,0.2)
		que.put( ("John", a) )
		que.put( ("Master Chief", b) )
		time.sleep(0.04)






