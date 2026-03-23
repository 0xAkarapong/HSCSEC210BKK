# NVDA Stock Price Tracker - Installation Guide

## Files Included
- `src/nvda-tracker.ts` - Main program
- `src/package.json` - Dependencies
- `nvda-tracker.service` - Systemd unit file

 ## Prerequisites

 1. Install Bun runtime:
 ```bash
 curl -fsSL https://bun.sh/install | bash
 ```

 2. Create dedicated user (run as root):
 ```bash
 useradd -r -s /bin/false nvda
 ```

 3. Copy Bun to system PATH (so the nvda user can access it):
 ```bash
 sudo cp ~/.bun/bin/bun /usr/local/bin/bun
 sudo chmod +x /usr/local/bin/bun
 ```

## Installation Steps

1. Copy files to server:
```bash
scp src/nvda-tracker.ts user@server:/opt/nvda-tracker/
scp src/package.json user@server:/opt/nvda-tracker/
scp nvda-tracker.service user@server:/tmp/
```

2. On server, as root:
```bash
# Create directory
mkdir -p /opt/nvda-tracker
mkdir -p /var/lib/nvda-tracker

# Copy files
mv /tmp/nvda-tracker.ts /opt/nvda-tracker/
mv /tmp/package.json /opt/nvda-tracker/

# Set ownership
chown -R nvda:nvda /opt/nvda-tracker
chown -R nvda:nvda /var/lib/nvda-tracker

# Install dependencies
cd /opt/nvda-tracker
sudo -u nvda bun install

# Install systemd service
cp /tmp/nvda-tracker.service /etc/systemd/system/
systemctl daemon-reload

# Start service
systemctl enable nvda-tracker
systemctl start nvda-tracker

# Check status
systemctl status nvda-tracker
```

## Viewing Logs
```bash
journalctl -u nvda-tracker -f
```

## Troubleshooting
- Check service status: `systemctl status nvda-tracker`
- View logs: `journalctl -u nvda-tracker`
- Restart: `systemctl restart nvda-tracker`

## How Restart Survival Works

The program persists price data to `/var/lib/nvda-tracker/data.json` every minute:

1. **On startup**: Checks if stored date matches today
2. **Previous day EOD**: If date changed but previous day had no closing price, prints EOD summary
3. **Continue tracking**: Loads previous opening/min/max and continues collecting new data

This ensures the EOD summary prints even if the program was restarted during the trading day.

## Bun Path Note

Bun installs to `~/.bun/bin/bun` which is not accessible to the `nvda` user.
The service file uses `/usr/local/bin/bun` - ensure you copy bun there before starting the service.

To find and copy Bun:
```bash
# Find Bun
which bun
# or
ls ~/.bun/bin/bun

# Copy to system path (required for service to work)
sudo cp ~/.bun/bin/bun /usr/local/bin/bun
sudo chmod +x /usr/local/bin/bun
```
