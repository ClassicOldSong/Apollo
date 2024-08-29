# Apollo

Apollo is a self-hosted desktop stream host for [Artemis(Moonlight Noir)](https://github.com/ClassicOldSong/moonlight-android). Offering low latency, cloud gaming server capabilities with support for AMD, Intel, and Nvidia GPUs for hardware encoding. Software encoding is also available. A web UI is provided to allow configuration and client pairing from your favorite web browser. Pair from the local server or any mobile device.

## Usage

Refer to LizardByte's documentation hosted on [Read the Docs](https://sushinestream.readthedocs.io/) for now.

Currently Virtual Display support is Windows only, Linux support is planned and will implement in the future.

## About Virtual Display

> [!WARNING]
> ***It is highly recommend to remove any other virtual display solutions from your system and Apollo/Sunshine config, to reduce confusions and compatibility issues.***

> [!NOTE]
> **TL;DR** Just treat your Artemis/Moonlight client like a dedicated PnP monitor with Apollo.

Apollo uses SudoVDA for virtual display. It featurs auto resolution and framerate matching for your Artemis/Moonlight clients. The virtual display is created upon the stream starts and removed once the app quits. **If you couldn't notice a new virtual display being added or removed when the stream starts/quits, then there might be a misconfiguration of the driver or you're still have other presisting virtual display connected.**

The virtual display works just like any physically attached monitors with SudoVDA, Windows will remember the monitor position config natively, there's completely no need for a super complicated solution to "fix" resolutions for your monitors. Unlike all other solutions that reuses one identity or generate a random one each time for any virtual display sessions, Apollo assigns a fixed identity for each Artemis client, or use a dedicated identity each app if using other Moonlight clients.

## Configuration for dual GPU laptops

Apollo supports dual GPUs seamlessly.

If you want to use your dGPU, just set the `Adapter Name` to your dGPU and enable `Headless mode` in `Audio/Video` tab, save and restart your computer. No dummy plug is needed any more, the image will be rendered and encoded directly from your dGPU.

## Troubleshooting

- **No virtual display added**
  - Ensure the SudoVDA driver is installed
- **Shows the same screen as main screen**
  - If you're using an external display for the first time, Windows might configure it as "Mirror mode" by default. Press <kbd>Meta + P</kbd> (or known as <kbd>Win + P</kbd>) and select "Extended", then **exit the app** (not only the stream) and start the app again. You only need to do this once.
- **Primary display changed to the virtual display after connection. I don't want that.**
  - Go to Apps and add one entry without any commands. Tick `Always use Virtual Display`, then untick `Set Virtual Display as Default`.
- **Resolution can't match client side anymore**
  - ***NEVER*** set screen rotation on virtual displays! Apollo can handle vertical display normally, there's no need to manually set screen rotatition if you're using [Artemis](https://github.com/ClassicOldSong/moonlight-android) with Apollo.
  - If you happen messed up with your monitor config:
    1. Disconnect ALL Artemis/Moonlight sessions
    2. Quit Apollo
    3. <kbd>Meta(Win) + R</kbd>, then type `regedit`, hit enter
    4. Delete ALL entries under
        - `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\GraphicsDrivers\Configuration`
        - `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\GraphicsDrivers\Connectivity`
        - `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\GraphicsDrivers\ScaleFactors`

    This will clear your monitor configuration cache.

    Then you're good to go!

## System Requirements

> **Warning**: This table is a work in progress. Do not purchase hardware based on this.

**Minimum Requirements**

| **Component** | **Description** |
|---------------|-----------------|
| GPU           | AMD: VCE 1.0 or higher, see: [obs-amd hardware support](https://github.com/obsproject/obs-amd-encoder/wiki/Hardware-Support) |
|               | Intel: VAAPI-compatible, see: [VAAPI hardware support](https://www.intel.com/content/www/us/en/developer/articles/technical/linuxmedia-vaapi.html) |
|               | Nvidia: NVENC enabled cards, see: [nvenc support matrix](https://developer.nvidia.com/video-encode-and-decode-gpu-support-matrix-new) |
| CPU           | AMD: Ryzen 3 or higher |
|               | Intel: Core i3 or higher |
| RAM           | 4GB or more |
| OS            | Windows: 10+ (Windows Server does not support virtual gamepads) |
|               | macOS: 12+ |
|               | Linux/Debian: 11 (bullseye) |
|               | Linux/Fedora: 39+ |
|               | Linux/Ubuntu: 22.04+ (jammy) |
| Network       | Host: 5GHz, 802.11ac |
|               | Client: 5GHz, 802.11ac |

**4k Suggestions**

| **Component** | **Description** |
|---------------|-----------------|
| GPU           | AMD: Video Coding Engine 3.1 or higher |
|               | Intel: HD Graphics 510 or higher |
|               | Nvidia: GeForce GTX 1080 or higher |
| CPU           | AMD: Ryzen 5 or higher |
|               | Intel: Core i5 or higher |
| Network       | Host: CAT5e ethernet or better |
|               | Client: CAT5e ethernet or better |

**HDR Suggestions**

| **Component** | **Description** |
|---------------|-----------------|
| GPU           | AMD: Video Coding Engine 3.4 or higher |
|               | Intel: UHD Graphics 730 or higher |
|               | Nvidia: Pascal-based GPU (GTX 10-series) or higher |
| CPU           | AMD: todo |
|               | Intel: todo |
| Network       | Host: CAT5e ethernet or better |
|               | Client: CAT5e ethernet or better |

## Integrations

SudoVDA: Virtual Display Adapter Driver used in Apollo

[Artemis](https://github.com/ClassicOldSong/moonlight-android): Integrated Virtual Display options control from client side

**NOTE**: Artemis currently supports Android only. Other platforms will come later.

## Support

Currently support is only provided via GitHub Issues.

## Downloads

[Releases](https://github.com/ClassicOldSong/Apollo/releases)

## Disclaimer

I got kicked from Moonlight and Sunshine's Discord server and banned from Sunshine's GitHub repo literally for helping people out.

![image](https://github.com/user-attachments/assets/f01fc57f-5199-4495-9b96-68cfa017b7ff)

This is what I got for finding a bug, opened an issue, getting no response, troubleshoot myself, fixed the issue myself, shared it by PR to the main repo hoping my efforts can help someone else during the maintenance gap.

Yes, I'm going away. [Apollo](https://github.com/ClassicOldSong/Apollo) and [Artemis(Moonlight Noir)](https://github.com/ClassicOldSong/moonlight-android) will no longer be compatible with OG Sunshine and OG Moonlight eventually, but they'll work even better with much more carefully designed features.

The Moonlight repo had stayed silent for 5 months, with nobody actually responding to issues, and people are getting totally no help besides the limited FAQ in their Discord server. I tried to answer issues and questions, solve problems within my ablilty but I got kicked out just for helping others. The funniest thing is, the repo starts updating after they got me banned!

**PRs for feature improvements are welcomed here unlike the main repo, your ideas are more likely to be appreciated and your efforts are actually being respected. We welcome people who can and willing to share their efforts, helping yourselves and other people in need.**

## License

GPLv3