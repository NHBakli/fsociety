all:
	$(MAKE) -C server
	$(MAKE) -C controller
	$(MAKE) -C implant

clean:
	$(MAKE) -C server clean
	$(MAKE) -C controller clean
	$(MAKE) -C implant clean
