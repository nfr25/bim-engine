CC      = gcc
TARGET  = bim.exe
SRCS    = bim.c
RES     = resources.res

INCLUDES = -I ../sqlite -I ./ui -I ./container -I ./bim

LDFLAGS  = -L ./lib -lsqlite3 -lm -lshlwapi -lgdi32 -luser32 -lcomctl32 \
           -lcairo -lcomdlg32 -lshp -ldwmapi 

LDFLAGS += $(shell pkg-config --libs librsvg-2.0 cairo)

CFLAGS   = -std=gnu11 $(INCLUDES)
CFLAGS += $(shell pkg-config --cflags librsvg-2.0 cairo)

all: $(TARGET)

$(RES): .\bim\bim_canvas.h app.ico
	windres --output-format=coff -i .\bim\bim_canvas.h -o $(RES)

$(TARGET): $(SRCS) $(RES)
	$(CC) $(CFLAGS) -o $(TARGET) $(RES) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(RES)