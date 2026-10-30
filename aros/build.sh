#!/bin/sh
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 John Knipper
# Explicit SDK selection; see README.md.
set -eu
exec python3 "$(dirname "$0")/build.py" "$@"
