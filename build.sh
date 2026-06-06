#!/bin/bash
# build.sh — bash wrapper, calls build_now.ps1 through PowerShell
# Usage: ./build.sh [-Tests] [-Debug] [-Clean]

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PS1_PATH="$SCRIPT_DIR/build_now.ps1"

# Convert bash args to PowerShell switch params
PS_ARGS=""
for arg in "$@"; do
    case "$arg" in
        -Tests|-Debug|-Clean)
            PS_ARGS="$PS_ARGS -$arg"
            ;;
        *)
            echo "Unknown argument: $arg"
            echo "Usage: ./build.sh [-Tests] [-Debug] [-Clean]"
            exit 1
            ;;
    esac
done

echo "Running: powershell -File \"$PS1_PATH\" $PS_ARGS"
echo ""

powershell -NoProfile -File "$PS1_PATH" $PS_ARGS
EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ]; then
    echo ""
    echo "Build completed successfully."
else
    echo ""
    echo "Build failed (exit code: $EXIT_CODE)"
fi

exit $EXIT_CODE
