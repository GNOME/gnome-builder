AC_DEFUN([YELP_HELP_INIT],
[
AC_REQUIRE([AC_PROG_LN_S])
m4_pattern_allow([AM_V_at])
m4_pattern_allow([AM_V_GEN])
m4_pattern_allow([AM_DEFAULT_VERBOSITY])

YELP_LC_MEDIA_LINKS=true
YELP_LC_DIST=true

for yelpopt in [$1]; do
  case $yelpopt in
    lc-media-links)    YELP_LC_MEDIA_LINKS=true ;;
    no-lc-media-links) YELP_LC_MEDIA_LINKS= ;;
    lc-dist)           YELP_LC_DIST=true ;;
    no-lc-dist)        YELP_LC_DIST= ;;
    *) AC_MSG_ERROR([Unrecognized [YELP_HELP_INIT] option $yelpopt"]) ;;
  esac
done;
AC_SUBST([YELP_LC_MEDIA_LINKS])
AC_SUBST([YELP_LC_DIST])

AC_ARG_WITH([help-dir],
            AS_HELP_STRING([--with-help-dir=DIR],
                           [path where help files are installed]),,
            [with_help_dir='${datadir}/help'])
HELP_DIR="$with_help_dir"
AC_SUBST(HELP_DIR)

AC_ARG_VAR([ITSTOOL], [Path to the `itstool` command])
AC_CHECK_PROG([ITSTOOL], [itstool], [itstool])
if test x"$ITSTOOL" = x; then
  AC_MSG_ERROR([itstool not found])
fi

AC_ARG_VAR([XMLLINT], [Path to the `xmllint` command])
AC_CHECK_PROG([XMLLINT], [xmllint], [xmllint])
if test x"$XMLLINT" = x; then
  AC_MSG_ERROR([xmllint not found])
fi

YELP_HELP_RULES='
HELP_ID ?=
HELP_POT ?=
HELP_FILES ?=
HELP_EXTRA ?=
HELP_MEDIA ?=
HELP_LINGUAS ?=

_HELP_LINGUAS = $(if $(filter environment,$(origin LINGUAS)),$(filter $(LINGUAS),$(HELP_LINGUAS)),$(HELP_LINGUAS))
_HELP_POTFILE = $(if $(HELP_POT),$(HELP_POT),$(if $(HELP_ID),$(HELP_ID).pot))
_HELP_POFILES = $(if $(HELP_ID),$(foreach lc,$(_HELP_LINGUAS),$(lc)/$(lc).po))
_HELP_MOFILES = $(patsubst %.po,%.mo,$(_HELP_POFILES))
_HELP_C_FILES = $(foreach f,$(HELP_FILES),C/$(f))
_HELP_C_EXTRA = $(foreach f,$(HELP_EXTRA),C/$(f))
_HELP_C_MEDIA = $(foreach f,$(HELP_MEDIA),C/$(f))
_HELP_LC_FILES = $(foreach lc,$(_HELP_LINGUAS),$(foreach f,$(HELP_FILES),$(lc)/$(f)))
_HELP_LC_STAMPS = $(foreach lc,$(_HELP_LINGUAS),$(lc)/$(lc).stamp)

_HELP_DEFAULT_V = $(if $(AM_DEFAULT_VERBOSITY),$(AM_DEFAULT_VERBOSITY),1)
_HELP_V = $(if $(V),$(V),$(_HELP_DEFAULT_V))
_HELP_LC_VERBOSE = $(_HELP_LC_VERBOSE_$(_HELP_V))
_HELP_LC_VERBOSE_ = $(_HELP_LC_VERBOSE_$(_HELP_DEFAULT_V))
_HELP_LC_VERBOSE_0 = @echo "  GEN    "$(dir [$]@);

all: $(_HELP_C_FILES) $(_HELP_C_EXTRA) $(_HELP_C_MEDIA) $(_HELP_LC_FILES) $(_HELP_POFILES)

.PHONY: pot
pot: $(_HELP_POTFILE)
$(_HELP_POTFILE): $(_HELP_C_FILES) $(_HELP_C_EXTRA) $(_HELP_C_MEDIA)
	$(AM_V_GEN)if test -d "C"; then d=; else d="$(srcdir)/"; fi; \
	$(ITSTOOL) -o "[$]@" $(foreach f,$(_HELP_C_FILES),"$${d}$(f)")

.PHONY: repo
repo: $(_HELP_POTFILE)
	$(AM_V_at)for po in $(_HELP_POFILES); do \
	  if test "x[$](_HELP_V)" = "x0"; then echo "  GEN    $${po}"; fi; \
	  msgmerge -q -o "$${po}" "$${po}" "$(_HELP_POTFILE)"; \
	done

$(_HELP_POFILES):
	$(AM_V_at)if ! test -d "$(dir [$]@)"; then mkdir "$(dir [$]@)"; fi
	$(AM_V_at)if test ! -f "[$]@" -a -f "$(srcdir)/[$]@"; then cp "$(srcdir)/[$]@" "[$]@"; fi
	$(AM_V_GEN)if ! test -f "[$]@"; then \
	  (cd "$(dir [$]@)" && \
	    $(ITSTOOL) -o "$(notdir [$]@).tmp" $(_HELP_C_FILES) && \
	    mv "$(notdir [$]@).tmp" "$(notdir [$]@)"); \
	else \
	  (cd "$(dir [$]@)" && \
	    $(ITSTOOL) -o "$(notdir [$]@).tmp" $(_HELP_C_FILES) && \
	    msgmerge -o "$(notdir [$]@)" "$(notdir [$]@)" "$(notdir [$]@).tmp" && \
	    rm "$(notdir [$]@).tmp"); \
	fi

$(_HELP_MOFILES): %.mo: %.po
	$(AM_V_at)if ! test -d "$(dir [$]@)"; then mkdir "$(dir [$]@)"; fi
	$(AM_V_GEN)msgfmt -o "[$]@" "$<"

$(_HELP_LC_FILES): $(_HELP_LINGUAS)
$(_HELP_LINGUAS): $(_HELP_LC_STAMPS)
$(_HELP_LC_STAMPS): %.stamp: %.mo
$(_HELP_LC_STAMPS): $(_HELP_C_FILES) $(_HELP_C_EXTRA)
	$(AM_V_at)if ! test -d "$(dir [$]@)"; then mkdir "$(dir [$]@)"; fi
	$(_HELP_LC_VERBOSE)if test -d "C"; then d="../"; else d="$(abs_srcdir)/"; fi; \
	mo="$(dir [$]@)$(patsubst %/$(notdir [$]@),%,[$]@).mo"; \
	if test -f "$${mo}"; then mo="../$${mo}"; else mo="$(abs_srcdir)/$${mo}"; fi; \
	(cd "$(dir [$]@)" && $(ITSTOOL) -m "$${mo}" $(foreach f,$(_HELP_C_FILES),$${d}/$(f))) && \
	touch "[$]@"

.PHONY: clean-help
mostlyclean-am: $(if $(HELP_ID),clean-help)
clean-help:
	rm -f $(_HELP_LC_FILES) $(_HELP_LC_STAMPS) $(_HELP_MOFILES)

EXTRA_DIST ?=
EXTRA_DIST += $(_HELP_C_EXTRA) $(_HELP_C_MEDIA)
EXTRA_DIST += $(if $(YELP_LC_DIST),$(foreach lc,$(HELP_LINGUAS),$(lc)/$(lc).stamp))
EXTRA_DIST += $(foreach lc,$(HELP_LINGUAS),$(lc)/$(lc).po)
EXTRA_DIST += $(foreach f,$(HELP_MEDIA),$(foreach lc,$(HELP_LINGUAS),$(wildcard $(lc)/$(f))))

distdir: distdir-help-files
distdir-help-files: $(_HELP_LC_FILES)
	@for lc in C $(if $(YELP_LC_DIST),$(HELP_LINGUAS)) ; do \
	  $(MKDIR_P) "$(distdir)/$$lc"; \
	  for file in $(HELP_FILES); do \
	    if test -f "$$lc/$$file"; then d=./; else d=$(srcdir)/; fi; \
	    cp -p "$$d$$lc/$$file" "$(distdir)/$$lc/" || exit 1; \
	  done; \
	done; \

.PHONY: check-help
check: check-help
check-help:
	for lc in C $(_HELP_LINGUAS); do \
	  if test -d "$$lc"; \
	    then d=; \
	    xmlpath="$$lc"; \
	  else \
	    d="$(srcdir)/"; \
	    xmlpath="$$lc:$(srcdir)/$$lc"; \
	  fi; \
	  for page in $(HELP_FILES); do \
	    echo "$(XMLLINT) --noout --noent --path $$xmlpath --xinclude $$d$$lc/$$page"; \
	    $(XMLLINT) --noout --noent --path "$$xmlpath" --xinclude "$$d$$lc/$$page"; \
	  done; \
	done


.PHONY: install-help
install-data-am: $(if $(HELP_ID),install-help)
install-help: $(_HELP_LC_FILES)
	@for lc in C $(_HELP_LINGUAS); do \
	  $(mkinstalldirs) "$(DESTDIR)$(HELP_DIR)/$$lc/$(HELP_ID)" || exit 1; \
	done
	@for lc in C $(_HELP_LINGUAS); do for f in $(HELP_FILES); do \
	  if test -f "$$lc/$$f"; then d=; else d="$(srcdir)/"; fi; \
	  helpdir="$(DESTDIR)$(HELP_DIR)/$$lc/$(HELP_ID)/"; \
	  if ! test -d "$$helpdir"; then $(mkinstalldirs) "$$helpdir"; fi; \
	  echo "$(INSTALL_DATA) $$d$$lc/$$f $$helpdir`basename $$f`"; \
	  $(INSTALL_DATA) "$$d$$lc/$$f" "$$helpdir`basename $$f`" || exit 1; \
	done; done
	@for f in $(_HELP_C_EXTRA); do \
	  lc=`dirname "$$f"`; lc=`basename "$$lc"`; \
	  if test -f "$$f"; then d=; else d="$(srcdir)/"; fi; \
	  helpdir="$(DESTDIR)$(HELP_DIR)/$$lc/$(HELP_ID)/"; \
	  if ! test -d "$$helpdir"; then $(mkinstalldirs) "$$helpdir"; fi; \
	  echo "$(INSTALL_DATA) $$d$$f $$helpdir`basename $$f`"; \
	  $(INSTALL_DATA) "$$d$$f" "$$helpdir`basename $$f`" || exit 1; \
	done
	@for f in $(HELP_MEDIA); do \
	  for lc in C $(_HELP_LINGUAS); do \
	    if test -f "$$lc$$f"; then d=; else d="$(srcdir)/"; fi; \
	    helpdir="$(DESTDIR)$(HELP_DIR)/$$lc/$(HELP_ID)/"; \
	    mdir=`dirname "$$f"`; \
	    if test "x$mdir" = "x."; then mdir=""; fi; \
	    if ! test -d "$$helpdir$$mdir"; then $(mkinstalldirs) "$$helpdir$$mdir"; fi; \
	    if test -f "$$d$$lc/$$f"; then \
	      echo "$(INSTALL_DATA) $$d$$lc/$$f $$helpdir$$f"; \
	      $(INSTALL_DATA) "$$d$$lc/$$f" "$$helpdir$$f" || exit 1; \
	    elif test "x$$lc" != "xC"; then \
	      if test "x$(YELP_LC_MEDIA_LINKS)" != "x"; then \
	        echo "$(LN_S) -f $(HELP_DIR)/C/$(HELP_ID)/$$f $$helpdir$$f"; \
	        $(LN_S) -f "$(HELP_DIR)/C/$(HELP_ID)/$$f" "$$helpdir$$f" || exit 1; \
	      fi; \
	    fi; \
	  done; \
	done

.PHONY: uninstall-help
uninstall-am: $(if $(HELP_ID),uninstall-help)
uninstall-help:
	for lc in C $(_HELP_LINGUAS); do for f in $(HELP_FILES); do \
	  helpdir="$(DESTDIR)$(HELP_DIR)/$$lc/$(HELP_ID)/"; \
	  echo "rm -f $$helpdir`basename $$f`"; \
	  rm -f "$$helpdir`basename $$f`"; \
	done; done
	@for f in $(_HELP_C_EXTRA); do \
	  lc=`dirname "$$f"`; lc=`basename "$$lc"`; \
	  helpdir="$(DESTDIR)$(HELP_DIR)/$$lc/$(HELP_ID)/"; \
	  echo "rm -f $$helpdir`basename $$f`"; \
	  rm -f "$$helpdir`basename $$f`"; \
	done
	@for f in $(HELP_MEDIA); do \
	  for lc in C $(_HELP_LINGUAS); do \
	    helpdir="$(DESTDIR)$(HELP_DIR)/$$lc/$(HELP_ID)/"; \
	    echo "rm -f $$helpdir$$f"; \
	    rm -f "$$helpdir$$f"; \
	  done; \
	done;
'
AC_SUBST([YELP_HELP_RULES])
m4_ifdef([_AM_SUBST_NOTMAKE], [_AM_SUBST_NOTMAKE([YELP_HELP_RULES])])
])
