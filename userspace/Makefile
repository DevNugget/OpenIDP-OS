.SUFFIXES:

APP_DIRS := $(shell find . -mindepth 2 -name Makefile -not -path "./build/*" -exec dirname {} \;)
APP_DIRS := $(patsubst ./%,%,$(APP_DIRS))

BUILD_DIR := build
NVME_IMG ?= ../nvme.img

.PHONY: all clean install list-apps $(APP_DIRS)

all: $(BUILD_DIR) $(APP_DIRS)

list-apps:
	@printf '%s\n' $(APP_DIRS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ... (Keep the find command from before) ...

$(APP_DIRS):
	@echo "--- Starting build for: $@ ---"
	$(MAKE) -C $@ all
	@echo "--- Finished build for: $@, now copying binary ---"
	@NAME=$$(basename $@); \
	if [ -f "$@/$$NAME.elf" ]; then \
		cp -v "$@/$$NAME.elf" "$(BUILD_DIR)/$$NAME.elf"; \
	elif [ -f "$@/bin/$$NAME.elf" ]; then \
		cp -v "$@/bin/$$NAME.elf" "$(BUILD_DIR)/$$NAME.elf"; \
	else \
		echo "[USERSPACE] Error: Could not find $$NAME.elf in $@"; \
		exit 1; \
	fi
	@echo "--- Successfully processed: $@ ---"

install: all
	@if [ ! -f "$(NVME_IMG)" ]; then \
		echo "[USERSPACE] NVMe image not found: $(NVME_IMG)"; \
		exit 1; \
	fi
	@echo "[USERSPACE] Mounting $(NVME_IMG) to temporary directory..."
	@TMP_MNT=$$(mktemp -d); \
	sudo mount -o loop "$(NVME_IMG)" "$$TMP_MNT"; \
	if [ $$? -ne 0 ]; then \
		echo "[USERSPACE] Error: Failed to mount image."; \
		rmdir "$$TMP_MNT"; \
		exit 1; \
	fi; \
	sudo mkdir -p "$$TMP_MNT/bin"; \
	for binary in $(BUILD_DIR)/*.elf; do \
		echo "[USERSPACE] installing $$binary -> $(NVME_IMG)/bin/"; \
		sudo cp "$$binary" "$$TMP_MNT/bin/"; \
	done; \
	echo "[USERSPACE] Unmounting and cleaning up..."; \
	sudo umount "$$TMP_MNT"; \
	rmdir "$$TMP_MNT"
	@echo "[USERSPACE] Installation complete."

clean:
	@for dir in $(APP_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	rm -rf $(BUILD_DIR)