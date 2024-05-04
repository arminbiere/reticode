all:
	../../enchex zerone.hex zerone.data
	../../asreti instructions.reti instructions.code
	../../emreti -s instructions.code zerone.data
clean:
	rm -f zerone.data instructions.code
