echo "Building TinyCache binary..."
make release
echo "Copying binary..."
sudo cp build/release/db /usr/bin/tinycache
sudo cp tinycache.ini /etc/systemd/system/tinycache.service
echo "Starting Tinycache..."
sudo systemctl daemon-reload
sudo systemctl enable --now tinycache
