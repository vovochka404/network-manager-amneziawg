#!/bin/bash
# Post-process POT file to use relative paths

POT_FILE="$1"
SOURCE_DIR="$2"

if [ -z "$POT_FILE" ] || [ -z "$SOURCE_DIR" ]; then
    echo "Usage: $0 <pot_file> <source_dir>"
    exit 1
fi

# Replace absolute paths with relative ones
sed -i "s|#: ${SOURCE_DIR}/|#: |g" "$POT_FILE"
