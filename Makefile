PACKAGES := openssl fuse
CXX = g++
CXX_FLAGS = $(shell pkg-config --cflags $(PACKAGES)) -Wall -ggdb -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=25 -DDEBUG -std=c++14
LDFLAGS := $(shell pkg-config --libs $(PACKAGES))

# Final binary
BIN = 3dsfuse-ex
# Put all auto generated stuff to this build dir.
BUILD_DIR = ./build

# List of all .cpp source files.
CPP = $(wildcard src/*.cpp)

# All .o files go to build dir.
OBJ = $(CPP:%.cpp=$(BUILD_DIR)/%.o)
# Gcc/Clang will create these .d files containing dependencies.
DEP = $(OBJ:%.o=%.d)

# Default target named after the binary.
$(BIN) : $(BUILD_DIR)/$(BIN)

# Actual target of the binary - depends on all .o files.
$(BUILD_DIR)/$(BIN) : $(OBJ)
	mkdir -p $(@D)
	$(CXX) $^ -o $@ $(LDFLAGS)

# Include all .d files
-include $(DEP)

# Build target for every single object file.
# The potential dependency on header files is covered
# by calling `-include $(DEP)`.
$(BUILD_DIR)/%.o : %.cpp
	mkdir -p $(@D)
	$(CXX) $(CXX_FLAGS) -MMD -c $< -o $@

.PHONY : clean
clean :
	-rm $(BUILD_DIR)/$(BIN) $(OBJ) $(DEP)
