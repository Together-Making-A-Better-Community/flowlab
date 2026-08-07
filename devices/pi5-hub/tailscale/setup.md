# Tailscale Setup — Pi 5 Hub

> Remote access to the Pi for administration (SSH, logs, service restarts). This is for you and any trusted collaborators, not for the public-facing dashboard — that goes through Cloudflare Tunnel (see [[Firmware Software Apps and Connectivity]]).

---

## What This Is For

Tailscale creates a private mesh network between your devices (laptop, phone, the Pi) so you can reach the Pi as if it were on your local network, from anywhere, without port forwarding or managing WireGuard keys by hand. It's built on WireGuard under the hood, but Tailscale handles key exchange and NAT traversal automatically.

**Use it for:** SSH into the Pi, checking `journalctl` logs remotely, restarting services, pulling a `git pull` from the field.

**Don't use it for:** giving community members or grant stakeholders access to the dashboard. That's a public-facing need — Cloudflare Tunnel is the right tool there, since it doesn't require the viewer to install anything.

---

## Install (on the Pi)

```bash
curl -fsSL https://tailscale.com/install.sh | sh
sudo tailscale up
```

Running `sudo tailscale up` prints a URL. Open it in a browser, log in with whatever account you want to own this Tailscale network (a free personal account is enough for one Pi and a couple of admin devices), and approve the device.

Once approved, confirm it's connected:

```bash
tailscale status
```

This lists every device on your Tailscale network and its Tailscale IP (looks like `100.x.x.x`). Note the Pi's IP here — that's what you'll SSH to from anywhere.

---

## Install (on your laptop / admin devices)

Download the Tailscale client for your OS from https://tailscale.com/download and log in with the same account used on the Pi. Once both devices show as connected in `tailscale status`, you can reach the Pi directly:

```bash
ssh pi@100.x.x.x
```

No VPN config file, no port forwarding on your router, no need to know the Pi's public IP.

---

## Locking It Down (Recommended Before Adding Collaborators)

By default, any device you approve on your Tailscale account can reach any other device on the network. If more than one person will have Tailscale access, set up an **ACL policy** to restrict what each device/person can reach — for example, a collaborator's laptop can reach the Pi over SSH, but nothing else.

This is optional for a single-person setup, but worth doing before handoff. ACLs are managed at https://login.tailscale.com/admin/acls and exported as a JSON file. If you set one up, save a copy here:

```
devices/pi5-hub/tailscale/acl.json
```

---

## Security Notes

- **Never commit an auth key to this repo.** If you later automate Pi setup with a pre-generated Tailscale auth key (useful for flashing a second Pi without manually approving it in the browser), that key goes in the same gitignored `config.yaml` pattern used for WiFi credentials and database secrets — never hardcoded in a script that gets committed.
- Tailscale requires an account (Google, GitHub, Microsoft, or email) to manage the network. This is separate from any Flow Lab service account — use whichever you're comfortable being the "owner" of this device network long-term, since revoking/re-adding devices happens through that account.
- If the Pi is ever offline for an extended period and needs re-provisioning, `sudo tailscale up` again will prompt a fresh approval — no stored keys to worry about being stale.

---

## Quick Reference

```bash
# Install
curl -fsSL https://tailscale.com/install.sh | sh
sudo tailscale up

# Check status / find device IPs
tailscale status

# SSH to the Pi from anywhere
ssh pi@<pi-tailscale-ip>

# Disconnect (if needed)
sudo tailscale down
```

---

## References
- [[Firmware Software Apps and Connectivity]] — Cloudflare Tunnel for public dashboard access, the complementary piece to this
- [[GitHub Version Management]] — where secrets/config conventions (gitignore pattern) are defined