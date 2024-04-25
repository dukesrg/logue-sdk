UNITS := $(notdir $(wildcard ../loguePAD?*-synth))

install: $(UNITS)

$(UNITS):
	build drumlogue/$@
	mv ../$@/*.drmlgunit .

clean:
	@rm *.drmlgunit
