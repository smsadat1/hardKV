echo "Building hardKV binary..."
make release
echo "Copying binary..."
sudo cp build/release/hkv /usr/bin/hkv
sudo cp hardkv.ini /etc/systemd/system/hardkv.service
echo "Starting hardKV..."
sudo systemctl daemon-reload
sudo systemctl enable --now hardkv
