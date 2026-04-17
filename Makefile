CC = gcc

CFLAGS = `pkg-config fuse3 --cflags`
LIBS   = `pkg-config fuse3 --libs`

TARGET = mini_unionfs

SRC = src/main.c src/utils.c src/operations.c

all:
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)

run:
	./$(TARGET)
