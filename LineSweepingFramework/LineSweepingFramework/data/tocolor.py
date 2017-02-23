import sys
import numpy as np
import math

fname = sys.argv[1]

with open(fname,"rb") as f:
	data = np.fromfile(f,np.int32)

MASK16 = 0xFFFF
outarr = np.zeros(shape=len(data),dtype=np.uint8)

for i,v in enumerate(data):
	ctr_all = v>>16 & MASK16
	ctr_in  = v & MASK16
	if ctr_all == 0:
		# may happen at borders
		percentage_in = 0
	else:
		percentage_in = ctr_in/ctr_all
	#assert(percentage_in <= 1)
	#assert(percentage_in >= 0)
	outarr[i] = round(percentage_in * 255)

outarr.tofile("data_u8_.bin")