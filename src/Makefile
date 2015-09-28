all: prog progd

progd: common
	$(MAKE) -C progd 

prog: common
	$(MAKE) -C prog 

common:
	$(MAKE) -C ../lib 

clean:
	$(MAKE) -C ../lib clean
	$(MAKE) -C prog clean
	$(MAKE) -C progd clean

.PHONY: common clean
