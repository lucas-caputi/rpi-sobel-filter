CC = g++
CFLAGS = `pkg-config --cflags opencv4` -Wall -Werror -O0 -lpthread
LDFLAGS = `pkg-config --libs opencv4`
TARGET1 = sobel_filter
TARGET1OBJFILES = main.o to442_filters.o

all: $(TARGET1)

$(TARGET1): $(TARGET1OBJFILES)
	$(CC) -o $(TARGET1) $(TARGET1OBJFILES) $(LDFLAGS)

main.o: main.cpp to442_filters.hpp
	$(CC) $(CFLAGS) -c -o $@ $<

to442_filters.o: to442_filters.cpp to442_filters.hpp
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET1OBJFILES) $(TARGET1)