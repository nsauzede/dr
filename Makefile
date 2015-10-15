TARGET=dr

CFLAGS=-Wall -Werror
CFLAGS+=-g -O0
CFLAGS+=-m32
LDFLAGS+=-m32

all:$(TARGET)

clean:
	$(RM) $(TARGET)
