import sys
import numpy as np
import math

fname = sys.argv[1]

with open(fname,"rb") as f:
	data = np.fromfile(f,np.int32)

size = int(round(math.pow(len(data),1/3)))
print("size: ",size)
assert(size*size*size == len(data))

data = data.reshape((size,size,size))

assert(data.ndim == 3)

MASK16 = 0xFFFF

def mkimg(imgf,data,size,i):
	for y in range(size):
		for x in range(size):
			if i==0:
				v = data[size//2,y,x]
			elif i==1:
				v = data[y,size//2,x]
			elif i==2:
				v = data[y,x,size//2]
			else:
				assert(False)
			ctr_all = v>>16 & MASK16
			ctr_in  = v & MASK16
			if ctr_all == 0:
				# may happen at borders
				percentage_in = 0
			else:
				percentage_in = ctr_in/ctr_all
			assert(percentage_in <= 1)
			assert(percentage_in >= 0)
			px = percentage_in * 255
			print("%d" % px, file=imgf)

with open("quer.pgm","w") as imgf:
	print("P2",file=imgf)
	print("%d %d"%(size,size*3),file=imgf)
	print("255",file=imgf)
	for i in range(3):
		mkimg(imgf,data,size,i)