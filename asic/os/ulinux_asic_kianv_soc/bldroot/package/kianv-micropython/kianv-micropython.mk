################################################################################
#
# kianv-micropython
#
# A custom build of MicroPython 1.19.1 (unix port) for the KianV RV32IMA
# noMMU SoC (gf180mcu variant). Threads, FFI, SSL, btree and sockets are
# disabled so the resulting binary links statically against uClibc on a
# FLAT-binfmt / no-shared-libs target. machine.mem32 stays available, and
# we additionally drop a tiny pure-Python helper module `kianv_gpio.py`
# under /usr/lib/micropython for direct MMIO access to the SoC's GPIO
# peripheral at 0x10600000.
#
################################################################################

KIANV_MICROPYTHON_VERSION = 1.19.1
KIANV_MICROPYTHON_SITE = $(call github,micropython,micropython,v$(KIANV_MICROPYTHON_VERSION))
KIANV_MICROPYTHON_LICENSE = MIT, BSD-1-Clause, BSD-3-Clause, Zlib
KIANV_MICROPYTHON_LICENSE_FILES = LICENSE
KIANV_MICROPYTHON_DEPENDENCIES = host-pkgconf host-python3

# Some architectures (incl. riscv32) need the setjmp-based register dump for GC.
KIANV_MICROPYTHON_CFLAGS = -DMICROPY_GCREGS_SETJMP=1

# Variant 'standard' keeps the machine module (machine.mem32 etc) which is
# the whole point of this build. We disable every feature that needs
# dynamic linking / threads.
KIANV_MICROPYTHON_MAKE_ENV = \
	$(TARGET_MAKE_ENV) \
	GIT_DIR=.

KIANV_MICROPYTHON_MAKE_OPTS = \
	VARIANT=standard \
	MICROPY_PY_BTREE=0 \
	MICROPY_PY_USSL=0 \
	MICROPY_PY_FFI=0 \
	MICROPY_PY_THREAD=0 \
	MICROPY_PY_SOCKET=0 \
	MICROPY_PY_TERMIOS=0 \
	MICROPY_USE_READLINE=0 \
	MICROPY_SSL_AXTLS=0 \
	MICROPY_SSL_MBEDTLS=0 \
	CROSS_COMPILE=$(TARGET_CROSS) \
	CFLAGS_EXTRA="$(KIANV_MICROPYTHON_CFLAGS)" \
	LDFLAGS_EXTRA="$(TARGET_LDFLAGS)" \
	CWARN=

define KIANV_MICROPYTHON_BUILD_CMDS
	$(KIANV_MICROPYTHON_MAKE_ENV) $(MAKE) -C $(@D)/mpy-cross
	$(KIANV_MICROPYTHON_MAKE_ENV) $(MAKE) -C $(@D)/ports/unix \
		$(KIANV_MICROPYTHON_MAKE_OPTS)
endef

define KIANV_MICROPYTHON_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/ports/unix/build-standard/micropython \
		$(TARGET_DIR)/usr/bin/micropython
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/lib/micropython
	$(INSTALL) -D -m 0644 package/kianv-micropython/kianv_gpio.py \
		$(TARGET_DIR)/usr/lib/micropython/kianv_gpio.py
endef

$(eval $(generic-package))
