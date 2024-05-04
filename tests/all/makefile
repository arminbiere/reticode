all:
	../../enchex zerone.hex zerone.data
	../../asreti instructions.reti instructions.code
	../../emreti -s instructions.code zerone.data
	../../asreti immediates.reti immediates.code
	../../emreti -s immediates.code
clean:
	rm -f zerone.data instructions.code immediates.code
