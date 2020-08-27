include ../tools/Makefile.efi

APP := hdd/efi/boot/bootx64.efi
OBJ :=	src/efi/grr_efi.o \
	src/hv/uart.o \
	src/hv/hv.o \
	src/hv/init.o

all: $(APP)

$(APP): $(OBJ)
	$(LD) $(LDFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f $(APP) $(OBJ)
