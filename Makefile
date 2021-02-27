TARGET = tfs
SRCS = main.cpp tfs.cpp
OBJS = $(patsubst %.cpp,build/%.o,$(SRCS))

CXX = g++
CXXFLAGS = --std=c++17 -Wall -Wextra -Wpedantic -Werror \
	-D_GNU_SOURCE \
	-DFUSE_USE_VERSION=31 \
	$(shell pkg-config fuse3 --cflags)
LDFLAGS =
LIBS = $(shell pkg-config fuse3 --libs)

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -rf build/ $(TARGET)

.PHONY: format
format:
	clang-format -verbose -i src/*

.PHONY: test
test: $(TARGET)
	@if [ ! -e /tmp/tfs-mount ]; then \
		mkdir -p /tmp/tfs-mount ; \
	fi
	@./$(TARGET) ./test/test.img /tmp/tfs-mount
	ls -lha /tmp/tfs-mount
	@sudo fusermount3 -u /tmp/tfs-mount

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

build/%.o: src/%.cpp
	@mkdir -p build/
	$(CXX) $(CXXFLAGS) -c -o $@ $<
