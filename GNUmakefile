OBJDIR := obj

TOP = .

QEMU := qemu-system-i386

#try to generate a unique GDB port
GDBPORT := $(shell expr `id -u` % 5000 + 25000)

CC := gcc -pipe
AS := as
AR := ar
LD := ld
OBJCOPY := objcopy
OBJDUMP := objdump
NM := nm

# Compiler flags
# -fno-builtin is required to avoid refs to undefined functions in the kernel.
# Only optimize to -O1 to discourage inlining, which complicates backtraces.
CFLAGS := -O1 -fno-builtin -I$(TOP) -MD
CFLAGS += -fno-omit-frame-pointer
CFLAGS += -Wall -Wno-format -Wno-unused -Werror -gstabs -m32
# -fno-tree-ch prevented gcc from sometimes reordering read_ebp() before
# mon_backtrace()'s function prologue on gcc version: (Debin 4.7.2-5) 4.7.2
CFLAGS += -fno-tree-ch

# Add -fno-stack-protector if the option exists.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# Common linker flags
LDFLAGS := -m elf_i386

# Linker flags for Yu-OS user programs
ULDFLAGS := -T user/user.ld

GCC_LIB := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

KERN_CFLAGS := $(CFLAGS) -gstabs
USER_CFLAGS := $(CFLAGS) -gstabs

# Include Makefrags for subdirectories
include boot/Makefrag
include kern/Makefrag
include lib/Makefrag
include user/Makefrag

QEMUOPTS = -hda $(OBJDIR)/kern/kernel.img -serial mon:stdio -gdb tcp::$(GDBPORT)
IMAGES = $(OBJDIR)/kern/kernel.img

qemu: $(IMAGES) pre-qemu
	$(QEMU) $(QEMUOPTS)

pre-qemu: .gdbinit

.gdbinit: .gdbinit.tmpl
	sed "s/localhost:1234/localhost:$(GDBPORT)/" <$^ >$@

qemu-gdb: $(IMAGES) pre-qemu
	@echo "***"
	@echo "*** Now run 'make gdb'." 1>&2
	@echo "***"
	$(QEMU) $(QEMUOPTS) -S

clean:
	rm -rf $(OBJDIR)