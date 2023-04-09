# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileContributor: Antonio Niño Díaz, 2023

# Defines
# -------

DSWIFI_MAJOR	:= 0
DSWIFI_MINOR	:= 4
DSWIFI_REVISION	:= 2
VERSION		:= $(DSWIFI_MAJOR).$(DSWIFI_MINOR).$(DSWIFI_REVISION)
VERSION_HEADER	:= include/dswifi_version.h

# Tools
# -----

CP		:= cp
INSTALL		:= install
MAKE		:= make
RM		:= rm -rf

# Verbose flag
# ------------

ifeq ($(VERBOSE),1)
V		:=
else
V		:= @
endif

# Targets
# -------

.PHONY: all arm7 arm9 clean docs install

all: $(VERSION_HEADER) arm9 arm7

$(VERSION_HEADER): Makefile
	@echo "#ifndef _dswifi_version_h_" > $@
	@echo "#define _dswifi_version_h_" >> $@
	@echo >> $@
	@echo "#define DSWIFI_MAJOR    $(DSWIFI_MAJOR)" >> $@
	@echo "#define DSWIFI_MINOR    $(DSWIFI_MINOR)" >> $@
	@echo "#define DSWIFI_REVISION $(DSWIFI_REVISION)" >> $@
	@echo >> $@
	@echo '#define DSWIFI_VERSION "'$(DSWIFI_MAJOR).$(DSWIFI_MINOR).$(DSWIFI_REVISION)'"' >> $@
	@echo >> $@
	@echo "#endif // _dswifi_version_h_" >> $@

arm9:
	@+$(MAKE) -f Makefile.arm9 --no-print-directory
	@+$(MAKE) -f Makefile.arm9 --no-print-directory DEBUG=1

arm7:
	@+$(MAKE) -f Makefile.arm7 --no-print-directory
	@+$(MAKE) -f Makefile.arm7 --no-print-directory DEBUG=1

clean:
	@echo "  CLEAN"
	@$(RM) $(VERSION_HEADER) lib build

docs:
	@echo "  DOXYGEN"
	doxygen Doxyfile

INSTALLDIR	?= /opt/blocksds/core/libs/dswifi
INSTALLDIR_ABS	:= $(abspath $(INSTALLDIR))

install: all
	@echo "  INSTALL $(INSTALLDIR_ABS)"
	@test $(INSTALLDIR_ABS)
	$(V)$(RM) $(INSTALLDIR_ABS)
	$(V)$(INSTALL) -d $(INSTALLDIR_ABS)
	$(V)$(CP) -r include lib LICENSE.txt $(INSTALLDIR_ABS)
