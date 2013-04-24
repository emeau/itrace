#libxml # cp -r $SDK/usr/include/libxml2/libxml/ $SDK/usr/include/libxml

BIN = /Developer/Platforms/iPhoneOS.platform/Developer/usr/bin
GCC_BIN = $(BIN)/gcc
#GCC = $(GCC_BASE) -arch armv6
GCC = $(GCC_BASE) -arch armv7
GCC_NATIVE = gcc
SDK=/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.sdk/

CFLAGS = 
GCC_BASE = $(GCC_BIN) -Os $(CFLAGS) -Wimplicit -isysroot $(SDK) -F$(SDK)System/Library/Frameworks -F$(SDK)System/Library/PrivateFrameworks
GCC_UNIVERSAL = $(GCC_BASE) -arch armv6 -arch armv7

all: itrace.dylib
mac:
	gcc -dynamiclib -lsubstrate -lxml2 -framework Foundation -o itrace_mac.dylib main.c utils.c
itrace.dylib:
	$(GCC_UNIVERSAL) -dynamiclib -lsubstrate -lxml2 -framework Foundation -o itrace.dylib main.c utils.c crc32.c

clean:
	rm -f *.o itrace.dylib
