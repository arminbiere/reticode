all:
	cat tutorial.reti
	../../asreti tutorial.reti | ../../decbin | ../../enchex | ../../disreti
	../../asreti tutorial.reti | ../../emreti -s
	../../ranreti 42 10
clean:
