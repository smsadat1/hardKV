#!/bin/bash
set -e

echo "Building hardKV binary..."
make release

echo "Creating directories..."
sudo mkdir -p /etc/hardkv
sudo mkdir -p /var/lib/hardkv
sudo mkdir -p /var/log/hardkv

echo "Installing configuration..."
sudo install -m 0644 hardkv.ini /etc/hardkv/hardkv.ini

echo "Installing binary..."
sudo install -m 0755 build/release/hkv /usr/bin/hardkv

echo "Installing systemd service..."
sudo install -m 0644 hardkv.system /etc/systemd/system/hardkv.service

echo "Setting up user..."
if ! id hardkv &>/dev/null; then
    sudo groupadd -r hardkv
    sudo useradd -r -g hardkv -s /usr/sbin/nologin -M hardkv
fi

echo "Setting permissions..."
sudo chown -R hardkv:hardkv /var/lib/hardkv /var/log/hardkv
sudo chown root:hardkv /etc/hardkv
sudo chmod 750 /etc/hardkv

echo "Starting hardKV..."
sudo systemctl daemon-reload
sudo systemctl enable --now hardkv

echo "HardKV installation complete."