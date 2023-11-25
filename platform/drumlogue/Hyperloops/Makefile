UNITS := $(notdir $(wildcard ../Hyperloop-*))

install: $(UNITS)

$(UNITS):
	build drumlogue/$@
	mv ../$@/*.drmlgunit .

clean:
	@rm *.drmlgunit
