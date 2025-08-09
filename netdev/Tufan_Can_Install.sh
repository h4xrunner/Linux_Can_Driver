#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE_NAME="netdev_echo"

echo "=== UART-to-CAN Bridge Driver Installation ==="

# Build the module
echo "Building module..."
cd "$SCRIPT_DIR"
make clean >/dev/null 2>&1 || true
make || { echo "ERROR: Build failed!"; exit 1; }

# Load CAN dependencies
echo "Loading CAN framework modules..."
for module in can can-dev can-raw vcan; do
    if ! lsmod | grep -q "^$module "; then
        echo "  Loading $module..."
        sudo modprobe "$module" || echo "  WARNING: Failed to load $module"
    else
        echo "  $module already loaded"
    fi
done

# Create vcan0 interface if it doesn't exist
echo "Setting up vcan0 interface..."
if ! ip link show vcan0 >/dev/null 2>&1; then
    echo "  Creating vcan0 interface..."
    sudo ip link add dev vcan0 type vcan || { echo "ERROR: Failed to create vcan0"; exit 1; }
    echo "  vcan0 created successfully"
else
    echo "  vcan0 already exists"
fi

# Bring vcan0 up
echo "Bringing vcan0 interface up..."
if ! ip link show vcan0 | grep -q "UP"; then
    sudo ip link set vcan0 up || { echo "ERROR: Failed to bring vcan0 up"; exit 1; }
    echo "  vcan0 brought up successfully"
else
    echo "  vcan0 already up"
fi

# Unload existing module if loaded
if lsmod | grep -q "^$MODULE_NAME "; then
    echo "Unloading existing $MODULE_NAME module..."
    sudo rmmod "$MODULE_NAME" || echo "  WARNING: Failed to unload existing module"
fi

# Load the new module
echo "Loading $MODULE_NAME module..."
sudo insmod "${MODULE_NAME}.ko" || { echo "ERROR: Failed to load module!"; exit 1; }

echo ""
echo "=== Installation Complete! ==="
echo ""
echo "Status check:"
echo "  Module loaded: $(lsmod | grep "$MODULE_NAME" >/dev/null && echo "✓ YES" || echo "✗ NO")"
echo "  vcan0 exists:  $(ip link show vcan0 >/dev/null 2>&1 && echo "✓ YES" || echo "✗ NO")"
echo "  vcan0 up:      $(ip link show vcan0 | grep -q UP && echo "✓ YES" || echo "✗ NO")"
echo ""
echo "Recent kernel logs:"
dmesg | grep "$MODULE_NAME" | tail -5
echo ""
echo "To test:"
echo "  candump vcan0 &"
echo "  cansend vcan0 123#DEADBEEF"
echo ""
echo "To uninstall:"
echo "  sudo rmmod $MODULE_NAME"
echo "  sudo ip link delete vcan0"
