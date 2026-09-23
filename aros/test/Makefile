# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: 2026 John Knipper
# Host tests of the AROS data model, report and providers (no AROS needed).
CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined
SRC = test_model.c ../model.c ../report.c ../provider_apple.c ../provider_hosted.c ../provider_raspi.c

test: build/test_model
	./build/test_model

build/test_model: $(SRC) ../model.h ../report.h ../collect.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Ishim -o $@ $(SRC)

clean:
	rm -rf build
.PHONY: test clean
