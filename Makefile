CC = gcc
.PHONY: clean

max1730_read:
	$(CC) src/max17320.c src/bms_ctl.c src/main.c -o $@ -lpigpio

clean:
	rm -f max1730_read