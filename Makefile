all: flash

trellis.bin : src/trellis.ino
	particle compile photon . --saveTo trellis.bin

flash : trellis.bin
	particle flash trellis1 trellis.bin

print : src/trellis.ino
	vim -c 'hardcopy > output.ps' -c quit src/trellis.ino && ps2pdf output.ps >output.pdf

