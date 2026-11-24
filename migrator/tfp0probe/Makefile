CC = $(shell xcrun --sdk iphoneos --find clang)
SDK = $(shell xcrun --sdk iphoneos --show-sdk-path)
LDID = $(shell which ldid)

CFLAGS := -isysroot $(SDK) -mios-version-min=12.0 -arch arm64e -arch arm64 \
	-Wall -Wno-unused-variable -Wno-unused-function

all: main.c
	$(CC) $(CFLAGS) -o tfp0probe main.c
	$(LDID) -Sents.xml tfp0probe

clean:
	@rm -f tfp0probe
