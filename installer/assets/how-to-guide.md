# How to Install ZapBox Web Installer on a VPS

This guide shows you how to deploy the ZapBox Web Installer on an Ubuntu VPS using Caddy as the web server and SFTP for file management.

## Prerequisites

- Ubuntu VPS with SSH access
- Domain or subdomain pointing to your VPS IP
- SSH key for authentication
- Visual Studio Code with SFTP extension

---

## 1. Server Setup

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
# Create web directory
sudo mkdir -p /var/www/zapbox

# Set permissions (replace 'your-username' with your Ubuntu username)
sudo chown -R your-username:www-data /var/www/zapbox
sudo chmod -R 755 /var/www/zapbox
```

---

## 3. Configure Caddy

Edit Caddy configuration:
```bash
sudo nano /etc/caddy/Caddyfile
```

Add this configuration (replace `your-subdomain.your-domain.com` with your actual domain):
```
# Configuration for ZapBox Web Installer
your-subdomain.your-domain.com {
    root * /var/www/zapbox
    file_server
    
    encode gzip
    
    header /firmware/* {
        Content-Type "application/octet-stream"
    }
}
```

Reload Caddy:
```bash
sudo systemctl reload caddy
```

---

## 4. Configure Firewall

```bash
# Allow SSH (adjust port if using custom port)
sudo ufw allow 22/tcp

# Or for custom SSH port:
sudo ufw allow your-custom-port/tcp

# Allow HTTP/HTTPS
sudo ufw allow 80/tcp
sudo ufw allow 443/tcp

# Enable firewall
sudo ufw enable
```

---

## 5. Setup SFTP in VS Code

### Install Extension
Install the **SFTP** extension by **Natizyskunk** in VS Code:
```
Extension ID: natizyskunk.sftp
```

### Create Configuration File
Create `.vscode/sftp.json` in your ZapBox project:

```json
{
    "name": "ZapBox VPS Server",
    "host": "YOUR_SERVER_IP",
    "protocol": "sftp",
    "port": 22,
    "username": "your-username",
    "remotePath": "/var/www/zapbox",
    "uploadOnSave": false,
    "useTempFile": false,
    "openSsh": false,
    "downloadOnOpen": false,
    "privateKeyPath": "C:\\Users\\YourUsername\\.ssh\\id_rsa",
    "passphrase": true,
    "context": "./installer",
    "syncOption": {
        "delete": false,
        "update": false
    }
}
```

**Important:** Replace the following placeholders:
- `YOUR_SERVER_IP` - Your VPS IP address
- `your-username` - Your Ubuntu username
- `C:\\Users\\YourUsername\\.ssh\\id_rsa` - Path to your SSH private key
- Port number if you use a custom SSH port

### Add to .gitignore
Add this line to your `.gitignore` to prevent committing sensitive data:
```
.vscode/sftp.json
```

---

## 6. Upload Files to Server

1. In VS Code Explorer, **right-click** on the `installer` folder
2. Select **"Upload Folder"** from the bottom of the context menu
3. Enter your SSH key passphrase when prompted
4. Files will be uploaded to `/var/www/zapbox` on your server

---

## 7. Update/Sync Files

### Manual Upload
- **Single file:** Right-click on file → `SFTP: Upload File`
- **Entire folder:** Right-click on `installer` folder → `Upload Folder`
- **Sync changes:** `Ctrl+Shift+P` → `SFTP: Sync Local → Remote`

### Workflow
1. Edit files in VS Code (local)
2. Save changes
3. Right-click → Upload when ready
4. Test at `https://your-subdomain.your-domain.com`

---

## 8. Verify Installation

1. Open your browser and navigate to your domain
2. Check that the ZapBox Web Installer loads correctly
3. Verify all assets (CSS, images, firmware files) are accessible
4. Test the favicon and social media preview tags

---

## Security Notes

- ✅ Use SFTP (port 22) instead of FTP for encrypted file transfer
- ✅ Never commit `.vscode/sftp.json` to Git
- ✅ Use SSH keys with passphrase protection
- ✅ Keep your SSH private key secure
- ✅ Caddy automatically provides HTTPS with Let's Encrypt certificates

---

## Troubleshooting

### Cannot connect via SFTP
- Check if SSH server is running: `sudo systemctl status ssh`
- Verify firewall allows SSH port: `sudo ufw status`
- Test SSH connection: `ssh -p YOUR_PORT username@YOUR_SERVER_IP`

### Caddy not serving files
- Check Caddy status: `sudo systemctl status caddy`
- View Caddy logs: `sudo journalctl -u caddy -f`
- Verify file permissions: `ls -la /var/www/zapbox`

### 404 errors for assets
- Ensure all files were uploaded
- Check file permissions (should be 644 for files, 755 for directories)
- Clear browser cache with `Ctrl+F5`

---

## Additional Resources

- [Caddy Documentation](https://caddyserver.com/docs/)
- [SFTP Extension](https://marketplace.visualstudio.com/items?itemName=Natizyskunk.sftp)
- [ZapBox Repository](https://github.com/AxelHamburch/ZapBox)

---

**Last updated:** 2026-01-01
