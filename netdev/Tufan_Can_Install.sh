#!/bin/bash
make
modprobe can
modprobe can-dev
modprobe can-raw
modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0


insmod netdev_echo.ko
