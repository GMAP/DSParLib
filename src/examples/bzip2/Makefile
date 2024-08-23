APP_NAME=bzip2

# .h files
H_DSPAR=$(wildcard ../../libs/dspar/*.hpp)

# Compiler and linker
CXX=mpic++

# Flags for compiler
CXX_FLAGS=-std=c++2a

# Include directories
INCLUDE_DIRS=-I../../../libs \
	-L $$HOME/.local/lib

# Link dependencies
LINK_DEPS=-lbz2

# Output path
OUT_DIR=./output

# Targets
all: compress decompress

compress: bzip2_compress.cpp $(H_DSPAR)
	@mkdir -p $(OUT_DIR)
	@echo "Compiling application: bzip2_compress.cpp"; \
	$(CXX) $(CXX_FLAGS) $(INCLUDE_DIRS) bzip2_compress.cpp -o $(OUT_DIR)/bzip2_compress.out $(LINK_DEPS)

decompress: bzip2_decompress.cpp $(H_DSPAR)
	@mkdir -p $(OUT_DIR)
	@echo "Compiling application: bzip2_decompress.cpp"; \
	$(CXX) $(CXX_FLAGS) $(INCLUDE_DIRS) bzip2_decompress.cpp -o $(OUT_DIR)/bzip2_decompress.out $(LINK_DEPS)

clean:
	rm -rf $(OUT_DIR)