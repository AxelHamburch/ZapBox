# How to Deploy the Lightbox Gallery on a VPS

This guide shows you how to deploy the ZapBox Lightbox Gallery on an Ubuntu VPS using Caddy as the web server and SFTP for file management.

## Prerequisites

- Ubuntu VPS with SSH access
- Subdomain pointing to your VPS IP (e.g. `lightbox.zapbox.space`)
- SSH key for authentication
- Visual Studio Code with SFTP extension

---

## 1. Server Setup

If the server is already running (e.g. for the ZapBox Installer), skip ahead to step 2.

### Install SSH Server (if not already installed)
```bash
sudo apt update
sudo apt install openssh-server
sudo systemctl enable ssh
sudo systemctl start ssh
```

### Install Caddy Web Server
```bash
sudo apt install -y debian-keyring debian-archive-keyring apt-transport-https
curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/gpg.key' | sudo gpg --dearmor -o /usr/share/keyrings/caddy-stable-archive-keyring.gpg
curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/debian.deb.txt' | sudo tee /etc/apt/sources.list.d/caddy-stable.list
sudo apt update
sudo apt install caddy
```

---

## 2. Create Directory Structure

```bash
# Create web directory for the lightbox galleries
sudo mkdir -p /var/www/lightbox

# Set permissions (replace 'your-username' with your Ubuntu username)
sudo chown -R your-username:www-data /var/www/lightbox
sudo chmod -R 755 /var/www/lightbox
```

The directory structure on the server mirrors the local `lightbox/` folder:

```
/var/www/lightbox/
    index.html          ← Overview page (links to galleries)
    favicon.webp
    grit/
        index.html
        pics/
            01.webp
            02.webp
            ...
    candy/
        index.html
        pics/
            01.webp
            02.webp
            ...
```

---

## 3. Configure Caddy

Edit the Caddy configuration:
```bash
sudo nano /etc/caddy/Caddyfile
```

Add a new block for the lightbox subdomain:
```
lightbox.zapbox.space {
    root * /var/www/lightbox
    file_server
    encode gzip
}
```

If a block for the main installer already exists, simply append this new block below it.

Reload Caddy:
```bash
sudo systemctl reload caddy
```

---

## 4. Configure Firewall

If the firewall is already configured for the main installer, nothing more is needed (ports 80 and 443 are already open).

If setting up fresh:
```bash
sudo ufw allow 22/tcp      # SSH
sudo ufw allow 80/tcp      # HTTP
sudo ufw allow 443/tcp     # HTTPS
sudo ufw enable
```

---

## 5. Setup SFTP in VS Code

### Install Extension
Install the **SFTP** extension by **Natizyskunk** in VS Code:
```
Extension ID: natizyskunk.sftp
```

### Configuration File
The ZapBox project already has `.vscode/sftp.json` configured with two profiles — one for the installer and one for the lightbox. It looks like this:

```json
[
    {
        "name": "ZapBox Installer (zapbox.space)",
        "host": "YOUR_SERVER_IP",
        "protocol": "sftp",
        "port": 22,
        "username": "your-username",
        "remotePath": "/var/www/zapbox",
        "privateKeyPath": "C:\\Users\\YourUsername\\.ssh\\id_rsa",
        "passphrase": true,
        "context": "./installer"
    },
    {
        "name": "ZapBox Lightbox (lightbox.zapbox.space)",
        "host": "YOUR_SERVER_IP",
        "protocol": "sftp",
        "port": 22,
        "username": "your-username",
        "remotePath": "/var/www/lightbox",
        "privateKeyPath": "C:\\Users\\YourUsername\\.ssh\\id_rsa",
        "passphrase": true,
        "context": "./lightbox"
    }
]
```

**Important:** Replace the following placeholders:
- `YOUR_SERVER_IP` — Your VPS IP address
- `your-username` — Your Ubuntu username
- `C:\\Users\\YourUsername\\.ssh\\id_rsa` — Path to your SSH private key
- Port number if you use a custom SSH port

### Keep out of Git
`.vscode/sftp.json` should be listed in `.gitignore` to avoid committing credentials:
```
.vscode/sftp.json
```

---

## 6. Add Gallery Images

Images must be placed in a `pics/` subfolder inside each gallery directory and named sequentially starting at `01.webp`:

```
lightbox/
    grit/
        pics/
            01.webp
            02.webp
            03.webp
            ...
    candy/
        pics/
            01.webp
            02.webp
            ...
```

The gallery pages automatically detect how many images exist — just add or remove files and re-upload.

---

## 7. Upload Files to Server

1. In VS Code Explorer, **right-click** on the `lightbox` folder
2. Select **"Upload Folder"** from the context menu
3. When prompted, select the profile **ZapBox Lightbox (lightbox.zapbox.space)**
4. Enter your SSH key passphrase when prompted
5. Files will be uploaded to `/var/www/lightbox` on your server

---

## 8. Update / Sync Files

### Manual Upload
- **Single file:** Right-click on file → `SFTP: Upload File`
- **Entire folder:** Right-click on `lightbox` folder → `Upload Folder`
- **Sync changes:** `Ctrl+Shift+P` → `SFTP: Sync Local → Remote`

### Typical Workflow
1. Add new images to `pics/` locally
2. Right-click `lightbox` → Upload Folder → select lightbox profile
3. Verify at `https://lightbox.zapbox.space`

---

## 9. Verify Installation

1. Open `https://lightbox.zapbox.space` — the overview page with gallery links should appear
2. Click a gallery — images should load automatically
3. Click an image — lightbox overlay should open with navigation (← →, Esc to close)
4. Verify favicon appears in the browser tab

---

## Security Notes

- ✅ Use SFTP (encrypted) instead of FTP
- ✅ Never commit `.vscode/sftp.json` to Git
- ✅ Use SSH keys with passphrase protection
- ✅ Caddy automatically provides HTTPS via Let's Encrypt

---

## Troubleshooting

### Cannot connect via SFTP
- Check SSH: `sudo systemctl status ssh`
- Verify firewall: `sudo ufw status`
- Test connection: `ssh -p YOUR_PORT username@YOUR_SERVER_IP`

### Caddy not serving files
- Check status: `sudo systemctl status caddy`
- View logs: `sudo journalctl -u caddy -f`
- Verify permissions: `ls -la /var/www/lightbox`

### Images not loading
- Ensure images are named `01.webp`, `02.webp`, … (no gaps in numbering)
- Check file permissions (should be 644 for files, 755 for directories)
- Clear browser cache with `Ctrl+F5`

### 404 on overview page
- Make sure `index.html` exists directly in `/var/www/lightbox/`

---

## Additional Resources

- [Caddy Documentation](https://caddyserver.com/docs/)
- [SFTP Extension](https://marketplace.visualstudio.com/items?itemName=Natizyskunk.sftp)
- [ZapBox Repository](https://github.com/AxelHamburch/ZapBox)

---

**Last updated:** 2026-03-14
