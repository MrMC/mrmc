# This file is sourced from xbmc/Makefile and tools/darwin/Support/makeinterfaces.command

FILEPATH := $(abspath $(dir $(MAKEFILE_LIST)))
GITVERFILE := ../VERSION
GIT = $(notdir $(shell which git))
ifneq ("$(wildcard $(FILEPATH)/OAuth2ClientInfo.json)","")
	OAUTHCLIENTSPATH := $(FILEPATH)/OAuth2ClientInfo.json
	OAUTHCLIENTS := $(shell cat ${OAUTHCLIENTSPATH})
endif

.PHONY: GitRevision $(FILEPATH)/.GitRevision
all: $(FILEPATH)/ClientPrivateInfo.cpp $(FILEPATH)/CompileInfo.cpp GitRevision
GitRevision: $(FILEPATH)/.GitRevision

$(FILEPATH)/.GitRevision:
	@if test -f $(GITVERFILE); then \
          GITREV=$$(cat $(GITVERFILE)) ;\
        elif test "$(GIT)" = "git" && test -d $(FILEPATH)/../.git ; then \
          if ! git diff-files --ignore-submodules --quiet -- || ! git diff-index --cached --ignore-submodules --quiet HEAD --; then \
            BUILD_DATE=$$(date -u "+%F"); \
            BUILD_SCMID=$$(git --no-pager log --abbrev=7 -n 1 --pretty=format:"%h-dirty"); \
            GITREV="$${BUILD_DATE}-$${BUILD_SCMID}" ;\
          else \
            BUILD_DATE=$$(git --no-pager log -n 1 --date=short --pretty=format:"%cd"); \
            BUILD_SCMID=$$(git --no-pager log --abbrev=7 -n 1 --pretty=format:"%h"); \
            GITREV="$${BUILD_DATE}-$${BUILD_SCMID}" ;\
          fi ;\
        else \
          GITREV="Unknown" ;\
        fi ;\
        [ -f $@ ] && OLDREV=$$(cat $@) ;\
        if test "$${OLDREV}" != "$${GITREV}"; then \
          echo $$GITREV > $@ ;\
        fi

$(FILEPATH)/ClientPrivateInfo.cpp: $(FILEPATH)/ClientPrivateInfo.cpp.in $(OAUTHCLIENTSPATH)
	sed -e 's/\@APP_CLIENTPRIVATE\@/$(OAUTHCLIENTS)/' $(FILEPATH)/ClientPrivateInfo.cpp.in > $(FILEPATH)/ClientPrivateInfo.cpp

$(FILEPATH)/CompileInfo.cpp: $(FILEPATH)/CompileInfoTemplate.cpp $(FILEPATH)/.GitRevision
	@GITREV=$$(cat $(FILEPATH)/.GitRevision) ;\
	sed -e "s/\@APP_SCMID\@/$$GITREV/" $(FILEPATH)/CompileInfoTemplate.cpp > $(FILEPATH)/CompileInfo.cpp ;\

