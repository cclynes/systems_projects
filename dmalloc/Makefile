# Default optimization level
OPT ?= -O0

# Flags
CXXFLAGS := -std=gnu++1z -W -Wall -Wshadow  -Wno-unused-command-line-argument -Werror -g $(OPT) $(DEFS) $(CXXFLAGS)
LDFLAGS := -no-pie

# Make sanitizers available as a compilation option ("make SAN=1")
#
# use SAN=1 to turn on all sanitizers (address, leak, undefined behavior)
#
# to turn on specific sanitizers, use:
# - ASAN=1 for the address sanitizer
# - LSAN=1 for the leak sanitizer
# - UBSAN=1 for the undefined behavior sanitizer
-include sanitizers.mk

# Get all of the test filenames
TESTS = $(patsubst %.cc,%,$(sort $(wildcard test[0-9][0-9][0-9].cc)))

TESTS_NO_ASAN := test005 test027 test028 test029 test030 test031 test032 test033 test034 test035 test036 test037 test038 test039 test040
TESTS_NO_LSAN := test002 test003 test004 test005 test006 test007 test009 test012 test013 test016 test017 test018 test019 test020 test021 test022 test023

TESTS_NEED_ASAN := $(filter-out $(TESTS_NO_ASAN),$(TESTS))

NOLSAN_OBJ := disable_lsan.o

all: $(TESTS)

# Link math library for static functions

LIBS = -lm

# Allow "make V=1" to print verbose compilation output (incl. all commands run)
ifneq ($(V),1)
# Define run function, which echoes its second and third argument and then
# lays its first and third out as a command
# https://www.gnu.org/software/make/manual/html_node/Call-Function.html
run = @$(if $(2),/bin/echo "  $(2) $(3)" &&,) $(1) $(3)
else
run = $(1) $(3)
endif

# these rules ensure dependencies are created
DEPCFLAGS = -MD -MF $(DEPSDIR)/$*.d -MP
DEPSDIR := .deps
BUILDSTAMP := $(DEPSDIR)/rebuildstamp
DEPFILES := $(wildcard $(DEPSDIR)/*.d)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif


# when the C compiler or optimization flags change, rebuild all objects
# if (has "check") and (is only target) => skip
# else, run the thing

ifeq ($(MAKECMDGOALS),) # Make with no targets
CHECK_DEPS := 1
else
# Otherwise, skip checking if goals only contains targets with "check"
CHECK_DEPS := $(intcmp $(words $(MAKECMDGOALS)),$(words $(filter check%,$(MAKECMDGOALS))),1,0,1)
endif

ifeq ($(CHECK_DEPS),1)
ifneq ($(strip $(DEP_CC)),$(strip $(CC) $(CPPFLAGS) $(CFLAGS) $(SANFLAGS) $(OPT)))
DEP_CC := $(shell mkdir -p $(DEPSDIR); echo >$(BUILDSTAMP); echo "DEP_CC:=$(CC) $(CPPFLAGS) $(CFLAGS) $(SANFLAGS) $(OPT)" >$(DEPSDIR)/_cc.d)
endif
ifneq ($(strip $(DEP_CXX)),$(strip $(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SANFLAGS) $(OPT)))
DEP_CXX := $(shell mkdir -p $(DEPSDIR); echo >$(BUILDSTAMP); echo "DEP_CXX:=$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SANFLAGS) $(OPT)" >$(DEPSDIR)/_cxx.d)
endif
endif

all:
	@echo "*** Run 'make check' to check your work."

# test%: dmalloc.o basealloc.o test%.o $(NOLSAN_OBJ)
# 	$(call run,$(CXX) $(CXXFLAGS) -o $@ $(filter-out $(NOLSAN_OBJ),$^) $(LDFLAGS) $(LIBS),LINK $@) $(if $(findstring $@,$(TESTS_NO_LSAN)),disable_lsan.o,)

# Rule template for compilation with sanitizers
define TEST_SAN
$1.o: $1.cc $$(BUILDSTAMP)
	$$(call run,$$(CXX) $$(CPPFLAGS) $$(CXXFLAGS) $$(SANFLAGS) -o $$@ -c,COMPILE,$$<)

$1: dmalloc.o basealloc.o  $$(NOLSAN_OBJ) $1.o
	$$(call run,$$(CXX) $$(CXXFLAGS) $$(SANFLAGS) -o $$@ $$(filter-out $$(NOLSAN_OBJ),$$^) $$(if $$(findstring $$@,$$(TESTS_NO_LSAN)),$$(NOLSAN_OBJ),) $$(LDFLAGS) $$(LIBS),LINK $$@)
endef

# Rule template for compilation without sanitizers
define TEST_NO_SAN
$1.o: $1.cc $$(BUILDSTAMP)
	$$(call run,$$(CXX) $$(CPPFLAGS) $$(CXXFLAGS) -o $$@ -c,COMPILE,$$<)

$1: dmalloc.nosan.o basealloc.nosan.o  $1.o
	$$(call run,$$(CXX) $$(CXXFLAGS) -o $$@ $$(LDFLAGS) $$^ $$(LIBS),LINK $$@)
endef

# Add rules for tests with and without sanitizers
$(foreach t,$(TESTS_NEED_ASAN),$(eval $(call TEST_SAN,$(t))))
$(foreach t,$(TESTS_NO_ASAN),$(eval $(call TEST_NO_SAN,$(t))))

basealloc.nosan.o: basealloc.cc $(BUILDSTAMP)
	$(call run,$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c,COMPILE,$<)

dmalloc.nosan.o: dmalloc.cc $(BUILDSTAMP)
	$(call run,$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c,COMPILE,$<)

%.o: %.cc $(BUILDSTAMP)
	$(call run,$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SANFLAGS) -o $@ -c,COMPILE,$<)

# Check all tests
check: $(patsubst %,run-%,$(TESTS))
	@echo "*** All tests succeeded!"

# Check a specific test... don't worry about perl
check-%:
	@any=false; good=true; for i in `perl check.pl -c30 -e "$*"`; do \
	    any=true; $(MAKE) run-$$i || good=false; done; \
	if $$any; then $$good; else echo "*** No such test" 1>&2; $$any; fi

JSONARG :=
JSON_FILE ?= /tmp/results.json
JSON ?= 0
ifeq ($(JSON),1)
JSONARG=-j $(JSON_FILE)
endif

checkall: $(TESTS)
	perl check.pl -c30 $(JSONARG)

# Run rule .. different than run function!
run-:
	@echo "*** No such test" 1>&2; exit 1

run-%: %
	@test -d out || mkdir out
	@perl check.pl -c30 -x $<

format:
	clang-format -style=Google -i $(FILES)


clean: clean-main
clean-main:
	$(call run,rm -f $(TESTS) *.o core *.core,CLEAN)
	$(call run,rm -rf out *.dSYM $(DEPSDIR))

MALLOC_CHECK_=0
export MALLOC_CHECK_

$(BUILDSTAMP):
	@mkdir -p $(@D)
	@echo >$@

always:
	@:

clean-hook:
	@:


.PRECIOUS: %.o
.PHONY: all clean clean-main format \
	run run- run% check  check-% checkall
