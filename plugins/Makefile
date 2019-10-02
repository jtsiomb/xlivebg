.PHONY: all
all: distort stars colcycle

.PHONY: clean
clean: clean-distort clean-stars clean-colcycle

.PHONY: install
install: install-distort install-stars install-colcycle

.PHONY: uninstall
uninstall: uninstall-distort uninstall-stars uninstall-colcycle

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

# stars plugin
.PHONY: stars
stars:
	$(MAKE) -C stars

.PHONY: clean-stars
clean-stars:
	$(MAKE) -C stars clean

.PHONY: install-stars
install-stars:
	$(MAKE) -C stars install

.PHONY: uninstall-stars
uninstall-stars:
	$(MAKE) -C stars uninstall

# colcycle plugin
.PHONY: colcycle
colcycle:
	$(MAKE) -C colcycle

.PHONY: clean-colcycle
clean-colcycle:
	$(MAKE) -C colcycle clean

.PHONY: install-colcycle
install-colcycle:
	$(MAKE) -C colcycle install

.PHONY: uninstall-colcycle
uninstall-colcycle:
	$(MAKE) -C colcycle uninstall

