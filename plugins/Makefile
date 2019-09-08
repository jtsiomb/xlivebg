.PHONY: all
all: distort

.PHONY: clean
clean: clean-distort

.PHONY: install
install: install-distort

.PHONY: uninstall
uninstall: uninstall-distort

# distort plugin
.PHONY: distort
distort:
	$(MAKE) -C distort

.PHONY: clean-distort
clean-distort:
	$(MAKE) -C distort clean

.PHONY: install-distort
install-distort:
	$(MAKE) -C distort install

.PHONY: uninstall-distort
uninstall-distort:
	$(MAKE) -C distort uninstall
