CXX      := clang++-6.0
CFLAGS   := --std=gnu++1z -g -Wall
LFLAGS   := -Wall
LIBS     := -lusb -lusb-1.0

ifneq (clean, $(filter clean, $(MAKECMDGOALS)))
  -include .build/debug
  -include $(patsubst %.cpp,.build/%.d,$(SRCS))
  ifneq ($(DEBUG),$(OLDDEBUG))
    override _ := $(shell $(MAKE) clean)
  endif
endif
ifeq (1,$(DEBUG))
  DFLAGS := -fsanitize=address -fno-omit-frame-pointer -O0
else
  DFLAGS := -O3
  CFLAGS += -DNDEBUG
  LFLAGS += -s
endif

SRCS     := $(shell echo *.cpp)

all: harmonizerc

.PHONY: clean
clean:
	rm -rf harmonizerc .build
	@[ "$(DEBUG)" = 1 ] && { mkdir -p .build; { echo 'DEBUG ?= 1'; echo 'override OLDDEBUG := 1'; } >.build/debug; } || :

harmonizerc: $(patsubst %.cpp,.build/%.o,$(SRCS)) .build/debug
	$(CXX) $(DFLAGS) $(LFLAGS) -o $@ $(patsubst %.cpp,.build/%.o,$(SRCS)) $(LIBS)

.build/%.o: %.cpp | .build/debug
	@mkdir -p .build
	$(CXX) -c -MP -MMD $(DFLAGS) $(CFLAGS) -o $@ $<

.build/debug:
	@mkdir -p .build
	@[ -n "$(DEBUG)" ] && { echo 'DEBUG ?= $(DEBUG)'; echo 'override OLDDEBUG = $(DEBUG)'; } >$@ || :
