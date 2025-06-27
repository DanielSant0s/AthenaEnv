libs:
	mkdir -p lib
	$(MAKE) -C OPCODE
	$(MAKE) -C OPCODE/Ice
	$(MAKE) -C ode/src 

install: libs
	$(MAKE) -C OPCODE install
	$(MAKE) -C OPCODE/Ice install
	$(MAKE) -C ode/src install

sample: libs
	$(MAKE) -C ode/test

clean:
	$(MAKE) -C OPCODE clean
	$(MAKE) -C OPCODE/Ice clean
	$(MAKE) -C ode/src clean
	$(MAKE) -C ode/test clean
	rm -r -f lib/
