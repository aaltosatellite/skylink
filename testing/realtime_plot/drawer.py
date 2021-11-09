from PyQt5 import QtWidgets, QtGui, QtCore
import numpy as np
from sigfig import round as sround
import time
from .abstraction import PlotLine, get_gridlines
import multiprocessing
import sys


bg_color1 = QtGui.QColor(0,50,50)
bg_color2 = QtGui.QColor(35,55,35)
bg_color3 = QtGui.QColor(0x20,0x45,0x30)
bg_color4 = QtGui.QColor(110,30,30)

no_pen = QtGui.QPen(QtCore.Qt.NoPen)
line_pen = QtGui.QPen(QtGui.QColor(200, 200, 200), 2.0)
axis_pen = QtGui.QPen(QtGui.QColor(200, 200, 200), 2.0)
grid_pen = QtGui.QPen(QtGui.QColor(200, 200, 200), 0.3)
dot_brush = QtGui.QBrush(QtGui.QColor(250,250,250))
bar_brush = QtGui.QBrush(QtGui.QColor(60,60,60))

line_pens = [
	QtGui.QPen(QtGui.QColor(255, 110, 110), 2.0),
	QtGui.QPen(QtGui.QColor(120, 255, 125), 2.0),
	QtGui.QPen(QtGui.QColor(110, 120, 255), 2.0),
	QtGui.QPen(QtGui.QColor(200, 200, 200), 2.0),
	QtGui.QPen(QtGui.QColor(200, 200, 200), 2.0),
	QtGui.QPen(QtGui.QColor(200, 200, 200), 2.0)
]

clrs = {0:"#ff5555", 1:"#55ff55", 2:"#5588ff"}

def get_color_name(i:int):
	if i in clrs:
		return clrs[i]
	return "#aaaaaa"





def _make_pathitem(plotline, pixwid, pixheight, scope):
	xscope = scope[0]
	yscope = scope[1]
	pathitem = QtWidgets.QGraphicsPathItem()
	if yscope[0] == yscope[1]:
		pix_per_y = 1.0
	else:
		pix_per_y = pixheight / (yscope[1] - yscope[0])
	if xscope[0] == xscope[1]:
		pix_per_x = 1.0
	else:
		pix_per_x = pixwid / (xscope[1] - xscope[0])
	Y = pixheight - plotline.cast_y_to_coordinates(yscope[0],  pix_per_y, 0)

	#X = plotline.cast_x_to_coordinates(xscope[0],  pix_per_x, 0)
	X = pixwid - plotline.cast_x_to_coordinates(xscope[0],  pix_per_x, 0)

	path = QtGui.QPainterPath( QtCore.QPoint(X[0], Y[0]) )
	for i in range(len(Y)):
		path.lineTo(X[i], Y[i])
	pathitem.setPath(path)
	pathitem.setPen(line_pen)
	return pathitem




class Graph2(QtWidgets.QGraphicsView):
	def __init__(self, parent=None, persistence=200, x_interval=1.0):
		super(Graph2, self).__init__(parent)
		#self.parent = parent
		self.persistence = persistence
		self.x_interval = x_interval
		self.scene0 = QtWidgets.QGraphicsScene()
		self.setScene(self.scene0)
		self.scene0.setBackgroundBrush(  QtGui.QBrush(bg_color1)  )
		self.scene0.setSceneRect(0,0,self.width()-12,self.height()-12)
		self.gridline_items = list()
		self.plotLines = dict()
		self.pens      = dict()
		self.pathitems = dict()
		self.top_margin = 0.1
		self.x_margin_l = 25
		self.x_margin_r = 10
		self.y_margin_u = 10
		self.y_margin_d = 25
		self.scope = ((0,1), (0,1))
		self.redraw()

	def _update_scope(self):
		if not self.plotLines:
			return

		old = tuple(self.scope)
		self.scope = (None,None)
		for name in self.plotLines:
			plotline = self.plotLines[name]
			self.scope = plotline.get_scope(self.scope, self.top_margin)
		if self.scope != old:
			return 1
		return 0


	def add_data(self, name, point):
		if not name in self.plotLines:
			self.plotLines[name] = PlotLine(self.persistence, self.x_interval)
			self.pens[name] = line_pens[ (len(self.plotLines)-1) % len(line_pens) ]
		plotline = self.plotLines[name]
		if type(point) in (tuple, list, np.ndarray):
			plotline.add_xy(point[0], point[1])
		else:
			plotline.add_y(point)
		changed = self._update_scope()
		if changed:
			#print("ping", self.scope)
			self.redraw()
		else:
			self.draw_name(name)


	def draw_name(self, name):
		plotline = self.plotLines[name]
		if name in self.pathitems:
			self.scene0.removeItem(self.pathitems[name])
		self.pathitems[name] = _make_pathitem(plotline, self.draw_width(), self.draw_height(), self.scope)
		self.pathitems[name].setPen(self.pens[name])
		self.pathitems[name].setPos(self.x_margin_l, self.y_margin_u)
		self.scene0.addItem(self.pathitems[name])


	def resizeEvent(self, QResizeEvent):
		super().resizeEvent(QResizeEvent)
		self.redraw()

	def draw_width(self):
		return self.scene0.width() - self.x_margin_l - self.x_margin_r

	def draw_height(self):
		return self.scene0.height() - self.y_margin_u - self.y_margin_d

	def get_span_x(self):
		return self.scope[0][1] - self.scope[0][0]

	def get_span_y(self):
		return self.scope[1][1] - self.scope[1][0]

	def pix_per_x(self):
		return self.draw_width() / self.get_span_x()

	def pix_per_y(self):
		return self.draw_height() / self.get_span_y()

	def clear(self):
		self.pathitems.clear()
		self.gridline_items.clear()
		self.pens.clear()
		self.plotLines.clear()
		self.scene0.clear()
		self.redraw()



	def redraw(self):
		#print("redraw")
		self.scene0.clear()
		self.pathitems.clear()
		self.gridline_items.clear()
		self.scene0.setSceneRect(0, 0, self.width() - 12, self.height() - 12)
		self._update_scope()
		if not self.plotLines:
			return

		for name in self.plotLines:
			self.draw_name(name)


		x_gridlines = get_gridlines(12, self.scope[0])
		y_gridlines = get_gridlines(8, self.scope[1])
		for y in y_gridlines:
			yp = self.y_margin_u + self.draw_height() - (y - self.scope[1][0]) * self.pix_per_y()
			gl_item = QtWidgets.QGraphicsLineItem(0, yp, self.x_margin_l + self.draw_width(), yp)
			gl_item.setPen(grid_pen)
			txt_item = QtWidgets.QGraphicsSimpleTextItem(str(sround(y,2)))
			txt_item.setParentItem(gl_item)
			txt_item.setPos(0,yp)
			txt_item.setPen(no_pen)
			txt_item.setBrush(dot_brush)
			self.scene0.addItem(gl_item)
			self.gridline_items.append(gl_item)

		for x in x_gridlines:
			#xp = self.draw_width() - (x - self.scope[0][0]) * self.pix_per_x() + self.x_margin_l
			xp = self.x_margin_l + (x - self.scope[0][0]) * self.pix_per_x()
			gl_item = QtWidgets.QGraphicsLineItem(xp, self.y_margin_u, xp, self.y_margin_u+self.draw_height())
			gl_item.setPen(grid_pen)
			txt_item = QtWidgets.QGraphicsSimpleTextItem(str(sround(x,2)))
			txt_item.setParentItem(gl_item)
			txt_item.setPos(xp , self.draw_height()+self.y_margin_u)
			txt_item.setPen(no_pen)
			txt_item.setBrush(dot_brush)
			self.scene0.addItem(gl_item)
			self.gridline_items.append(gl_item)









class MW(QtWidgets.QWidget):
	def __init__(self, que, retention, interval_ms):
		super(MW, self).__init__()
		self.interval = interval_ms
		self.addque = que
		self.lo = QtWidgets.QVBoxLayout()
		self.setLayout(self.lo)
		self.name_widget = QtWidgets.QWidget()
		self.name_hbox = QtWidgets.QHBoxLayout()
		self.name_widget.setLayout(self.name_hbox)
		self.G = Graph2(self, retention, interval_ms)
		self.layout().addWidget(self.G)
		self.layout().addWidget(self.name_widget)
		self.previous = dict()
		self.names    = dict()

		self.loopcount = 0
		self.starttime = time.perf_counter()
		self.timer = QtCore.QTimer()
		self.timer.setInterval(self.interval)
		self.timer.timeout.connect(self._add_loop)
		self.timer.start()


	def keyPressEvent(self, a0: QtGui.QKeyEvent) -> None:
		super(MW, self).keyPressEvent(a0)
		print(a0.key())
		if a0.key() == 32:
			if self.timer.isActive():
				self.timer.stop()
			else:
				self.timer.start(self.interval)
		if a0.key() == 67:
			self.G.clear()
			self.names.clear()
			self.previous.clear()
			self.lo.removeWidget(self.name_widget)
			self.name_hbox = QtWidgets.QHBoxLayout()
			self.name_widget = QtWidgets.QWidget()
			self.name_widget.setLayout(self.name_hbox)
			self.lo.addWidget(self.name_widget)


	def _add_loop(self):
		self.loopcount += 1
		while not self.addque.empty():
			name, p = self.addque.get_nowait()
			if not (type(p) in (int, float, np.float64)):
				continue
			if not (type(name) in (str, bytes, int)):
				continue
			self.previous[name] = p

		for name in self.previous.keys():
			#t = time.perf_counter()
			v = self.previous[name]
			self.G.add_data(name, v )

			if not name in self.names:
				self.names[name] = True
				label = QtWidgets.QLabel(str(name))
				color_name = get_color_name(len(self.names) -1)
				label.setStyleSheet("background-color: {}; border: 1px solid black;".format(color_name))
				self.name_hbox.addWidget(label)


def _mw_start(que, retention, interval_ms):
	app = QtWidgets.QApplication(sys.argv)
	mw = MW(que, retention, interval_ms)
	mw.show()
	mw.resize(800,600)
	app.exec_()


def create_realplot(retention, interval_ms):
	mgr = multiprocessing.Manager()
	que = mgr.Queue(1000)
	p = multiprocessing.Process(target=_mw_start, args=(que, retention, interval_ms))
	p.start()
	return que


