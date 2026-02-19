UNITS := $(notdir $(wildcard ../FM?*-osc))

install: $(UNITS)

$(UNITS):
	build nts-1_mkii/$@
	mv ../$@/*.nts1mkiiunit .

clean:
	@rm *.nts1mkiiunit
