all: flash

trellis.bin : src/trellis.ino
	particle compile photon . --saveTo trellis.bin

flash : trellis.bin
	particle flash trellis1 trellis.bin

