# --- toolchain ---
CXX      ?= clang++
CPPFLAGS := -Iinclude            # so #include "tinydb/..." works
CXXFLAGS := -std=c++20 -Wall -Wextra -O2
LDFLAGS  :=

# --- OS-specific setup (macOS needs SDK + libc++) ---
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  SDK := $(shell xcrun --show-sdk-path)
  CXXFLAGS += -isysroot $(SDK) -stdlib=libc++
  LDFLAGS  += -isysroot $(SDK) -stdlib=libc++
  # If your env has rogue /usr/local/include headers, uncomment the next two lines:
  # CXXFLAGS += -nostdinc -nostdinc++ -isystem $(SDK)/usr/include/c++/v1 -isystem $(SDK)/usr/include
  # LDFLAGS  += -nostdinc -nostdinc++ -isystem $(SDK)/usr/include/c++/v1 -isystem $(SDK)/usr/include
endif

# --- project layout ---
APP      := build/tinydb
SRC_DIRS := src .
SRCS     := $(wildcard $(addsuffix /*.cpp,$(SRC_DIRS)))
OBJS     := $(patsubst %.cpp,build/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)

# ensure build/ subdirs exist (mirrors SRC_DIRS)
$(shell mkdir -p $(addprefix build/,$(SRC_DIRS)))

# --- default target ---
all: $(APP)

# link
$(APP): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

# compile (with dep files)
build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

# convenience
DB_PATH := data/tiny.db

run: $(APP)
	mkdir -p $(dir $(DB_PATH))
	./$(APP) $(DB_PATH)

clean:
	rm -rf build

.PHONY: all run clean

# include auto-generated header deps (safe if none exist)
-include $(DEPS)
