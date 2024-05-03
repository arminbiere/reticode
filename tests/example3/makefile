all:
	cat example3.hex
	../../enchex example3.hex example3.bin
	../../decbin example3.bin | ../../enchex | ../../decbin
clean:
	rm -f example3.bin
