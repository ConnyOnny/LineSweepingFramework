s = """template<> const char* getTypeName<%s>() {
	return "%s";
}"""

tns = ["char","uchar","short","ushort","int","uint","long","ulong","float","double"]
nums = ["", 2, 4, 8, 16]

for t in tns:
	for n in nums:
		print(s%("cl_"+t+str(n), t+str(n)))
