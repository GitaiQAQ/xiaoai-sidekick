# Reference: SSH Access to a Supported Speaker

The project has been verified on Xiaomi XiaoAI Speaker Pro (LX06), ROM 1.94.13, with the SSH patch described by the upstream project. This is a device-specific prerequisite, not a general installation method. SSH access alone does not prove that the factory assistant, Mi Home, mesh gateway, or firmware recovery works.

Use the address assigned by your own network and keep the SSH host-key file outside the repository. Example: `ssh -o HostKeyAlgorithms=+ssh-rsa -o UserKnownHostsFile=<private-known-hosts> root@<speaker-ip>`. Do not commit host keys, passwords, private keys, or partition readbacks.

After connecting, confirm the model and firmware using the read-only checks documented by the upstream patch. Do not use this guide as authorization to flash a device; review the current deployment and recovery instructions first.
