all:

VERSION=$(shell cat ../../.hos_version)
MINORVERSION=$(shell cat ../../.hos_minor_version)
FULLVERSION=$(shell cat ../../.hos_full_version)
CODENAME:=$(shell lsb_release -sc)
REPO=../..
PACKVER:=$(FULLVERSION)-$(shell lsb_release -si)$(shell lsb_release -sr)
DEBS:=$(subst .csv,,$(wildcard *.csv))
STORAGE:=debian/$(CODENAME)

pack:
	test -n "$(VERSION)"
	test -n "$(MINORVERSION)"
	test -n "$(FULLVERSION)"
	@echo "Packaging:"
	@echo " - Package version: $(PACKVER)"
	@echo " - Packages: $(DEBS)"
	rm -Rf debian
	mkdir -p $(STORAGE)
	for p in $(DEBS); do rm -f $(REPO)/$${p}*.deb; (cd $(REPO); ./packaging/deb/htchdebian-mkdeb packaging/deb/$${p}.csv $(PACKVER)) && mv $(REPO)/$${p}*.deb $(STORAGE)/;done
