all: prog progd test

progd: common
	$(MAKE) -C progd 

prog: common
	$(MAKE) -C prog 

test:
	$(MAKE) -C test 

common:
	$(MAKE) -C ../lib 

clean:
	$(MAKE) -C ../lib clean
	$(MAKE) -C prog clean
	$(MAKE) -C progd clean
	$(MAKE) -C test clean

.PHONY: FORCE common clean prog progd test
