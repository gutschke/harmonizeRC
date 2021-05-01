CXX      := clang++-6.0
CFLAGS   := --std=gnu++1z -g -Wall
LFLAGS   := -Wall
LIBS     := -lusb -lusb-1.0

ifeq (1,$(DEBUG))
  DFLAGS := -fsanitize=address -fno-omit-frame-pointer -O0
else
  DFLAGS := -O3
  CFLAGS += -DNDEBUG
  LFLAGS += -s
endif

SRCS     := $(shell echo *.cpp)

all: harmonizerc

clean:
	rm -rf harmonizerc .build

harmonizerc: $(patsubst %.cpp,.build/%.o,$(SRCS))
	$(CXX) $(DFLAGS) $(LFLAGS) -o $@ $^ $(LIBS)

.build/%.o: %.cpp
	@mkdir -p .build
	$(CXX) -c -MP -MMD $(DFLAGS) $(CFLAGS) -o $@ $<
-include $(patsubst %.cpp,.build/%.d,$(SRCS))
