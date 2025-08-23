# --- toolchain ---
CXX      ?= clang++          
CPPFLAGS := -Iinclude        # header root so #include "tinydb/..." works
CXXFLAGS := -std=c++20 -Wall -Wextra -O2
LDFLAGS  :=

# --- project layout ---
APP      := build/tinydb
SRC_DIRS := src .        # will pick up apps/main.cpp, src/*.cpp, etc.
SRCS     := $(wildcard $(addsuffix /*.cpp,$(SRC_DIRS)))
OBJS     := $(patsubst %.cpp,build/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)

# --- db location ---
DB_PATH := data/tiny.db

# make sure build/ subdirs exist
$(shell mkdir -p $(addprefix build/,$(SRC_DIRS)))

# --- default target ---
all: $(APP)

# link
$(APP): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDFLAGS) -o $@

# compile (with dep files)
build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

# convenience
run: $(APP)
	mkdir -p $(dir $(DB_PATH))
	./$(APP) $(DB_PATH)

clean:
	rm -rf build

.PHONY: all run clean

# include auto-generated header deps (safe if none exist)
-include $(DEPS)

