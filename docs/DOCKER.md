# Deploying with Dockge / Docker Compose

This setup runs `sin_ded` as a **native Linux container** (Debian slim, ~60MB, no Wine) managed easily via **Dockge**.

---

## 1. Directory Structure in Dockge

In your Dockge stacks directory (e.g. `/opt/stacks/sin-server/`):

```text
sin-server/
├── compose.yaml
├── Dockerfile
├── docker-entrypoint.sh
└── data/
    └── base/              <-- Copy your licensed Steam pak*.sin files here
        ├── pak0.sin
        ├── pak1.sin
        └── pak2.sin
```

---

## 2. Setting Up in Dockge

1. Open your **Dockge Web UI** and click **"+ Compose"**.
2. Name the stack: `sin-server`.
3. Paste the `compose.yaml`:
   ```yaml
   services:
     sin-dedicated:
       container_name: sin-server
       build:
         context: .
         dockerfile: Dockerfile
       restart: unless-stopped
       environment:
         - SERVER_NAME=My Dockge SiN Server
         - PORT=27910
         - MAP=biodm
         - MAX_CLIENTS=8
         - DEATHMATCH=1
         - SV_MASTER1=master.shavargo.com:27900
       ports:
         - "27910:27910/udp"
       volumes:
         - ./data/base:/sin/base:ro
   ```
4. Copy `Dockerfile` and `docker-entrypoint.sh` into the `/opt/stacks/sin-server/` directory on the host.
5. Copy your `pak0.sin`, `pak1.sin`, and `pak2.sin` from your Steam install (`steamapps/common/SiN 1 Multiplayer/base`) into `./data/base/`.
6. Click **"Deploy"** in Dockge!

---

## 3. Network / Port Forwarding

* **Dockge Host**: Ensure port `27910/udp` is allowed through the host firewall:
  ```bash
  sudo ufw allow 27910/udp
  ```
* **Router / WAN**: If you want players outside your local network to connect, forward port `27910/udp` on your home/office router to your physical Linux box's LAN IP.
* **Master Server**: The container will automatically send heartbeats to `master.shavargo.com:27900`.
