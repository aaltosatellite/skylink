from PySide2 import QtWidgets, QtGui, QtCore
import time
from .abstraction import XYVector, get_gridlines
import multiprocessing, threading
import sys, pickle
import numpy as np


POISON_PILL = "__KYS__"

bg_color1 = QtGui.QColor(0,35,35)
bg_color2 = QtGui.QColor(0,50,50)
bg_color3 = QtGui.QColor(35,55,35)
bg_color4 = QtGui.QColor(0x20,0x45,0x30)
bg_color5 = QtGui.QColor(110,30,30)

no_pen = QtGui.QPen(QtCore.Qt.NoPen)
line_pen = QtGui.QPen(QtGui.QColor(200, 200, 200), 2.0)
axis_pen = QtGui.QPen(QtGui.QColor(200, 200, 200), 2.0)
grid_pen = QtGui.QPen(QtGui.QColor(200, 200, 200), 0.3)
dot_brush = QtGui.QBrush(QtGui.QColor(250,250,250))
bar_brush = QtGui.QBrush(QtGui.QColor(60,60,60))

line_pens = [
	QtGui.QPen(QtGui.QColor(255, 110, 110), 1.2),	#red
	QtGui.QPen(QtGui.QColor(120, 255, 125), 1.2),	#green
	QtGui.QPen(QtGui.QColor(110, 120, 255), 1.2),	#blue
	QtGui.QPen(QtGui.QColor(255, 255, 100), 1.2),	#yellow
	QtGui.QPen(QtGui.QColor(110, 255, 255), 1.2),	#teal
	QtGui.QPen(QtGui.QColor(255, 110, 255), 1.2),	#purple
	QtGui.QPen(QtGui.QColor(255, 180, 000), 1.2),	#orange
	QtGui.QPen(QtGui.QColor(150, 150, 150), 1.2)	#grey
]


def siground(x, sig):
	x = float(x)
	f = np.format_float_scientific(x, unique=False, precision=sig)
	a,b = f.split("e")
	ee = int(b)
	return round(x, (-ee)+(sig-1))


def de_singularitize_scope(scope):
	if scope == (None,None):
		scope = (0,1),(0,1)
	if scope[0][0] == scope[0][1]:
		scope = ((0,1), scope[1])
	if scope[1][0] == scope[1][1]:
		scope = (scope[0], (0,1))
	return scope

def _make_pathitem(xyvector, xscope, yscope, pix_per_x, pix_per_y, pixheight, pixwidth):
	pathitem = QtWidgets.QGraphicsPathItem()
	#xfrom, xto, origin_x, pix_per_x, origin_y, pix_per_y
	X,Y = xyvector.cast_to_coordinates(xscope[0], xscope[1], xscope[0], pix_per_x, yscope[0], pix_per_y)
	Y = pixheight - Y
	X = pixwidth - X
	if len(X) == 0:
		return pathitem
	path = QtGui.QPainterPath( QtCore.QPoint(X[0], Y[0]) )
	for i in range(len(Y)):
		path.lineTo(X[i], Y[i])
	pathitem.setPath(path)
	pathitem.setPen(line_pen)
	return pathitem





class Graph2(QtWidgets.QGraphicsView):
	def __init__(self, parent=None, timespan=10.0):
		super(Graph2, self).__init__(parent)
		#self.parent = parent
		self.timespan = timespan
		self.scene0 = QtWidgets.QGraphicsScene()
		self.setScene(self.scene0)
		self.scene0.setBackgroundBrush(  QtGui.QBrush(bg_color1)  )
		self.scene0.setSceneRect(0,0,self.width()-12,self.height()-12)
		self.vectors 	= dict()
		self.pathitems  = dict()
		self.pens  		= dict()
		self.top_margin = 0.1
		self.x_margin_l = 40
		self.x_margin_r = 10
		self.y_margin_u = 10
		self.y_margin_d = 24
		self.yscope = (0,1)
		self.t_max = 0.0
		self.redraw()

	def draw_width(self):
		return self.scene0.width() - self.x_margin_l - self.x_margin_r

	def draw_height(self):
		return self.scene0.height() - self.y_margin_u - self.y_margin_d

	def get_span_x(self):
		return self.timespan

	def get_xscope(self):
		return self.t_max - self.timespan, self.t_max

	def get_span_y(self):
		return self.yscope[1] - self.yscope[0]

	def pix_per_x(self):
		return self.draw_width() / self.get_span_x()

	def pix_per_y(self):
		return self.draw_height() / self.get_span_y()

	def _update_yscope(self):
		old = self.yscope
		self.yscope = None
		for name in self.vectors:
			self.yscope = self.vectors[name].get_y_partscope(self.get_xscope(), self.yscope, self.top_margin)
		if self.yscope is None:
			self.yscope = (0,1)
		if self.yscope[0] == self.yscope[1]:
			self.yscope = (0,1)
		return self.yscope != old

	def set_timespan(self, timespan):
		self.timespan = timespan
		self.redraw()

	def add_data(self, name, x, y):
		if not name in self.vectors:
			vector = XYVector(8000, (None,None))
			self.vectors[name] = vector
			self.pens[name] = line_pens[(len(self.vectors) - 1) % len(line_pens)]
		xyvector = self.vectors[name]
		xyvector.add_xy(x,y)
		self.t_max = x
		yscope_changed = self._update_yscope()
		if yscope_changed:
			self.redraw()
		else:
			self.draw_name(name)


	def draw_name(self, name):
		xyvector = self.vectors[name]
		if name in self.pathitems:
			self.scene0.removeItem( self.pathitems[name])
			del self.pathitems[name]
		self.pathitems[name] = _make_pathitem(xyvector, self.get_xscope(), self.yscope,
											  self.pix_per_x(), self.pix_per_y(),  self.draw_height(), self.draw_width())
		self.pathitems[name].setPen(self.pens[name])
		self.pathitems[name].setPos(self.x_margin_l, self.y_margin_u)
		self.scene0.addItem(self.pathitems[name])


	def resizeEvent(self, QResizeEvent):
		super().resizeEvent(QResizeEvent)
		self.redraw()


	def clear(self):
		self.pathitems.clear()
		self.pens.clear()
		self.vectors.clear()
		self.scene0.clear()
		self.redraw()


	def redraw(self):
		#print("redraw")
		self.scene0.clear()
		self.pathitems.clear()
		self.scene0.setSceneRect(0, 0, self.width() - 12, self.height() - 12)
		self._update_yscope()
		if not self.vectors:
			return

		for name in self.vectors:
			self.draw_name(name)

		x_gridlines = get_gridlines(12, (0, self.timespan))
		y_gridlines = get_gridlines(8, self.yscope)
		for y in y_gridlines:
			yp = self.y_margin_u + self.draw_height() - (y - self.yscope[0]) * self.pix_per_y()
			gl_item = QtWidgets.QGraphicsLineItem(0, yp, self.x_margin_l + self.draw_width(), yp)
			gl_item.setPen(grid_pen)
			txt_item = QtWidgets.QGraphicsSimpleTextItem(str(siground(y,2)))
			txt_item.setParentItem(gl_item)
			txt_item.setPos(0,yp)
			txt_item.setPen(no_pen)
			txt_item.setBrush(dot_brush)
			self.scene0.addItem(gl_item)

		for x in x_gridlines:
			#xp = self.draw_width() - (x - self.scope[0][0]) * self.pix_per_x() + self.x_margin_l
			xp = self.x_margin_l + x * self.pix_per_x()
			gl_item = QtWidgets.QGraphicsLineItem(xp, self.y_margin_u, xp, self.y_margin_u+self.draw_height())
			gl_item.setPen(grid_pen)
			txt_item = QtWidgets.QGraphicsSimpleTextItem(str(siground(x,2)))
			txt_item.setParentItem(gl_item)
			txt_item.setPos(xp , self.draw_height()+self.y_margin_u)
			txt_item.setPen(no_pen)
			txt_item.setBrush(dot_brush)
			self.scene0.addItem(gl_item)









class MW(QtWidgets.QWidget):
	def __init__(self, que, timespan, interval_ms):
		super(MW, self).__init__()
		self.addque = que
		self.lock = threading.RLock()
		self.lo = QtWidgets.QGridLayout()
		self.setLayout(self.lo)

		self.notice_label = QtWidgets.QLabel("")
		self.notice_label.setStyleSheet("background-color: #aaaaaa; border: 1px solid black;")

		self.tools_widget = QtWidgets.QWidget()
		self.tools_layout = QtWidgets.QGridLayout()
		self.tools_widget.setLayout(self.tools_layout)
		self.itv_label = QtWidgets.QLabel("")
		self.itv_label.setStyleSheet("background-color: #aaaaaa; border: 1px solid black;")

		save_btn = QtWidgets.QPushButton("save data")
		save_btn.setMaximumWidth(80)
		save_btn.clicked.connect(self.saveFile)
		self.store_btn = QtWidgets.QPushButton("")
		self.store_btn.setMaximumWidth(160)
		self.store_btn.clicked.connect(self.store_toggle)
		self.tools_layout.addWidget(self.itv_label, 0, 0, 1, 1)
		self.tools_layout.addWidget(save_btn , 0, 1, 1, 1)
		self.tools_layout.addWidget(self.store_btn , 0, 2, 1, 1)
		self.tools_widget.setSizePolicy(QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Minimum)

		self.G = Graph2(self, timespan=timespan)
		self.G.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)

		self.name_widget = QtWidgets.QWidget()
		self.name_box = QtWidgets.QGridLayout()
		self.name_widget.setLayout(self.name_box)

		self.lo.addWidget(self.tools_widget, 0,0, 1,1)
		self.lo.addWidget(self.notice_label, 1,0, 1,1)
		self.lo.addWidget(self.G, 2,0, 1,1)
		self.lo.addWidget(self.name_widget, 3,0, 1,1 )
		self.names    		= set()
		self.latest_values 	= dict()
		self.data_vectors   = dict()

		self.pull_loop = threading.Thread(target=self._que_pull_loop, args=tuple(), daemon=True)
		self.pull_loop.start()

		self.loopcount = 0
		self.storage_on = False
		self.storage_count = 0
		self.starttime = time.perf_counter()
		self.timer = QtCore.QTimer()
		self.timer.setInterval(interval_ms)
		self.timer.timeout.connect(self._loop)
		self.timer.start()




	def saveFile(self):
		w = QtWidgets.QFileDialog.getSaveFileName(self)
		path = w[0]
		if not path:
			return
		with self.lock:
			dump = pickle.dumps(self.data_vectors)
			#self.data_vectors.clear()
			#self.storage_count = 0
		f = open(path, "wb")
		f.write(dump)
		f.close()



	def keyPressEvent(self, a0: QtGui.QKeyEvent) -> None:
		super(MW, self).keyPressEvent(a0)
		if a0.key() == 32: #SPACE - pause
			if self.timer.isActive():
				self.timer.stop()
			else:
				self.timer.start(self.timer.interval())
		if a0.key() == 67: #C - clear
			self._clear()

		if a0.key() == 70: #F - fullscreen
			if not self.isFullScreen():
				self.showFullScreen()
			else:
				self.showNormal()
		if a0.key() == 87: #W - increase refresh speed
			inter = self.timer.interval()
			inter = max(inter-2, 20)
			self.timer.setInterval(inter)
			self._set_interval_txt()
		if a0.key() == 83: #S - decrease refresh speed
			inter = self.timer.interval()
			inter = min(inter+2, 500)
			self.timer.setInterval(inter)
			self._set_interval_txt()
		if a0.key() == 68: #A
			self.G.set_timespan(self.G.timespan*0.95)
		if a0.key() == 65: #D
			self.G.set_timespan(self.G.timespan*1.05)






	def store_toggle(self):
		self.storage_on = not self.storage_on
		self._set_store_txt()

	def _clear(self):
		self.G.clear()
		self.names.clear()
		self.latest_values.clear()
		self.notice_label.setText("")
		self.lo.removeWidget(self.name_widget)
		self.name_box = QtWidgets.QGridLayout()
		self.name_widget = QtWidgets.QWidget()
		self.name_widget.setLayout(self.name_box)
		self.lo.addWidget(self.name_widget)
		self.data_vectors.clear()
		self.storage_count = 0

	def _set_store_txt(self):
		if self.storage_on:
			self.store_btn.setText("Storage ON. {}".format(self.storage_count))
			self.store_btn.setStyleSheet("background-color: #66ff66;")
		else:
			self.store_btn.setText("Storage OFF. ({})".format(self.storage_count))
			self.store_btn.setStyleSheet("background-color: #ff6666;")

	def _set_interval_txt(self):
		self.itv_label.setText("Interval: "+str(self.timer.interval()))


	def store_data(self, name, val):
		if not name in self.data_vectors:
			self.data_vectors[name] = list()
		self.data_vectors[name].append((time.perf_counter(), val))
		self.storage_count += 1


	def _loop(self):
		self.loopcount += 1
		self._set_interval_txt()
		self._set_store_txt()
		with self.lock:
			t = time.perf_counter()
			for name in self.latest_values.keys():
				v = self.latest_values[name]
				self.G.add_data(name, t, v)
				if not name in self.names:
					self.names.add(name)
					label = QtWidgets.QLabel(str(name)[:32])
					color = self.G.pens[name].color()
					rgb = (color.toRgb().red(), color.toRgb().green(), color.toRgb().blue())
					rgbhex =  "#" + hex(rgb[2] + (rgb[1]<<8) + (rgb[0]<<16))[2:]
					#color_name = get_color_name(len(self.names) -1)
					label.setStyleSheet("background-color: {}; border: 1px solid black;".format(rgbhex))
					n = len(self.names) - 1
					r = n // 4
					c = n % 4
					self.name_box.addWidget(label, r, c, 1,1)


	def _que_pull_loop(self):
		while True:
			try:
				k = self.addque.get()
			except:
				sys.exit(0)
			if k == POISON_PILL:
				sys.exit(0)
			try:
				name, val = k
				name = str(name)
				val = float(val)
			except:
				continue
			with self.lock:
				self.latest_values[name] = val
				if self.storage_on:
					self.store_data(name, val)






def _mw_start(que, retention, interval_ms, tile):
	app = QtWidgets.QApplication(sys.argv)
	mw = MW(que, retention, interval_ms)

	if tile and hasattr(tile, "__len__") and (len(tile) == 3) and (type(x) == int for x in tile):
		print("tiling")
		nr = tile[0]
		nc = tile[1]
		r = tile[2] % nc
		c = tile[2] // nr
		iw = 1900 // nc
		ih = 1060 // nr
		mw.move(c*iw +1, r*ih +1)
		mw.resize(iw,ih)
	else:
		print("not tiling")
		mw.resize(900,600)
	mw.show()
	app.exec_()


def create_realplot(retention_time_s, interval_ms, tile=None):
	mgr = multiprocessing.Manager()
	que = mgr.Queue(1000)
	p = multiprocessing.Process(target=_mw_start, args=(que, retention_time_s, interval_ms, tile))
	p.start()
	return que


