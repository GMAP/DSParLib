APP_NAME=eye-detector

# .h files
H_DSPAR=$(wildcard ../../libs/dspar/*.hpp)

# Compiler and linker
CXX=mpic++

# Flags for compiler
CXX_FLAGS=-std=c++11

# Include directories
INCLUDE_DIRS=-I../../../libs \
	-I/usr/include/opencv4/ \
	-L $$HOME/.local/lib

# Link dependencies
LINK_DEPS=-lopencv_core -lopencv_objdetect -lopencv_features2d -lopencv_imgproc -lopencv_videoio

# Output path
OUT_DIR=./output

# Targets
all: eye-detector

eye-detector: eye-detector.cpp $(H_DSPAR)
	@mkdir -p $(OUT_DIR)
	@echo "Compiling application: eye-detector.cpp"; \
	$(CXX) $(CXX_FLAGS) $(INCLUDE_DIRS) eye-detector.cpp -o $(OUT_DIR)/eye-detector.out $(LINK_DEPS)

clean:
	rm -rf $(OUT_DIR)