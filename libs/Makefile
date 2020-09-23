.PHONY: all
all: imago treestore

.PHONY: clean
clean: clean-imago clean-treestore

# imago library
.PHONY: imago
imago:
	$(MAKE) -C imago

.PHONY: clean-imago
clean-imago:
	$(MAKE) -C imago clean

# treestore library
.PHONY: treestore
treestore:
	$(MAKE) -C treestore

.PHONY: clean-treestore
clean-treestore:
	$(MAKE) -C treestore clean
