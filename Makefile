CC = g++
CXXFLAGS = -Wall -std=c++23 -O2 -Wno-unused-result
DEBUG_FLAGS = -std=c++23 -Wall -g -DDEBUG
LDFLAGS = 

bin = a.out

SRC_DIR = ./src

SRCS = $(notdir $(wildcard $(SRC_DIR)/*.cpp))
OBJS = $(SRCS:.cpp=.o)

all: $(bin)

%.o: $(SRC_DIR)/%.cpp
ifdef DEBUG
	$(CC) $(DEBUG_FLAGS) -c $< -o $@ -MD $(LDFLAGS)
else
	$(CC) $(CXXFLAGS) -c $< -o $@ -MD $(LDFLAGS)
endif

$(bin): $(OBJS)
ifdef DEBUG
	$(CC) $(DEBUG_FLAGS) $(OBJS) -o $@
else
	$(CC) $(CXXFLAGS) $(OBJS) -o $@
endif

.PHONY: clean all
clean:
	rm -f $(bin) *.o *.d


-include $(OBJS:.o=.d)