# Differential oracle for the 68000 core.
#
#   make check      build and compare against golden
#   make sanitize   the same run under ASan and UBSan
#   make dump       write the per-opcode lines to oracle.txt
#   make golden     re-record golden (only when a change is meant to
#                   alter behaviour, and only with the reason in the
#                   commit message)

CC      ?= cc
MUSASHI := ../../src/3rdparty/musashi
CFLAGS  ?= -O1 -g -Wall
SAN     := -fsanitize=address,undefined

SRC := $(MUSASHI)/m68kcpu.c $(MUSASHI)/m68kops.c opcode_oracle.c
DEP := $(SRC) $(MUSASHI)/m68k.h $(MUSASHI)/m68kcpu.h \
       $(MUSASHI)/m68kconf.h $(MUSASHI)/m68kops.h

all: check

oracle: $(DEP)
	$(CC) $(CFLAGS) -I$(MUSASHI) -o $@ $(SRC) -lm

oracle_san: $(DEP)
	$(CC) $(CFLAGS) $(SAN) -I$(MUSASHI) -o $@ $(SRC) -lm

check: oracle
	@got=$$(./oracle); want=$$(cat golden); \
	if [ "$$got" = "$$want" ]; then \
		echo "musashi opcode oracle: ok ($$got)"; \
	else \
		echo "musashi opcode oracle: MISMATCH"; \
		echo "  golden $$want"; \
		echo "  got    $$got"; \
		echo "  run 'make dump' here and on a known-good build,"; \
		echo "  then diff the two oracle.txt to see which encodings moved"; \
		exit 1; \
	fi

sanitize: oracle_san
	@./oracle_san >/dev/null

dump: oracle
	./oracle --dump > oracle.txt
	@echo "wrote oracle.txt"

golden: oracle
	./oracle > golden
	@echo "re-recorded golden: $$(cat golden)"

clean:
	rm -f oracle oracle_san oracle.txt

.PHONY: all check sanitize dump golden clean
