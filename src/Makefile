CFLAGS := $(CFLAGS) -I$(BUILDDIR) -Isrc/ -g -DYYENABLE_NLS=0

# Sources in this directory
DIR_SOURCES := nsgenbind.c utils.c webidl-ast.c nsgenbind-ast.c ir.c \
	duk-libdom.c duk-libdom-interface.c duk-libdom-dictionary.c \
	duk-libdom-common.c duk-libdom-generated.c

SOURCES := $(SOURCES) $(BUILDDIR)/nsgenbind-parser.c $(BUILDDIR)/nsgenbind-lexer.c $(BUILDDIR)/webidl-parser.c $(BUILDDIR)/webidl-lexer.c

$(BUILDDIR)/%-lexer.c $(BUILDDIR)/%-lexer.h: src/%-lexer.l
	$(VQ)$(ECHO) "    FLEX: $<"
	$(Q)$(FLEX) --outfile=$(BUILDDIR)/$(*F)-lexer.c --header-file=$(BUILDDIR)/$(*F)-lexer.h $<

$(BUILDDIR)/%-lexer.c: $(BUILDDIR)/%-parser.h

# Bison 3.0 and later require api.prefix in curly braces
# Bison 2.6 and later require api.prefix
# Bison 2.5 and earlier require name-prefix switch
bisonvsn := $(word 4,$(shell $(BISON) --version))
bisonmaj := $(word 1,$(subst ., ,$(bisonvsn)))
bisonmin := $(word 2,$(subst ., ,$(bisonvsn)))
ifeq ($(bisonmaj),1)
  BISON_DEFINES = --name-prefix=$(*F)_
else
  ifeq ($(bisonmaj),2)
    ifneq ($(findstring $(bisonmin),"0 1 2 3 4 5"),)
      BISON_DEFINES = --name-prefix=$(*F)_
    else
      BISON_DEFINES = --define=api.prefix=$(*F)_
    endif
  else
    BISON_DEFINES = --define=api.prefix={$(*F)_}
  endif
endif

$(BUILDDIR)/%-parser.c $(BUILDDIR)/%-parser.h: src/%-parser.y
	$(VQ)$(ECHO) "   BISON: $<"
	$(Q)$(BISON) -d -t $(BISON_DEFINES) --report=all --output=$(BUILDDIR)/$(*F)-parser.c --defines=$(BUILDDIR)/$(*F)-parser.h $<

# Grab the core makefile
include $(NSBUILD)/Makefile.subdir
