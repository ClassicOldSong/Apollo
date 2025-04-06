# Apollo

Apollo is a self-hosted desktop stream host for [Artemis(Moonlight Noir)](https://github.com/ClassicOldSong/moonlight-android). Offering low latency, native client resolution, cloud gaming server capabilities with support for AMD, Intel, and Nvidia GPUs for hardware encoding. Software encoding is also available. A web UI is provided to allow configuration and client pairing from your favorite web browser. Pair from the local server or any mobile device.

Major features:

- [x] Built-in Virtual Display with HDR support that matches the resolution/framerate config of your client automatically
- [x] Permission management for clients
- [x] Clipboard sync
- [x] Commands for client connection/disconnection (checkout [Auto pause/resume games](https://github.com/ClassicOldSong/Apollo/wiki/Auto-pause-resume-games))
- [x] Input only mode

## Usage

Refer to LizardByte's documentation hosted on [Read the Docs](https://docs.lizardbyte.dev/projects/sunshine) for now.

Currently Virtual Display support is Windows only, Linux support is planned and will be implemented in the future.

## About Permission System

Check out the [Wiki](https://github.com/ClassicOldSong/Apollo/wiki/Permission-System)

> [!NOTE]
> The **FIRST** client paired with Apollo will be granted with FULL permissions, then other newly paired clients will only be granted with `View Streams` and `List Apps` permission. If you encounter `Permission Denied` error when trying to launch any app, go check the permission for that device and grant `Launch Apps` permission. The same applies to the situation when you find that you can't move mouse or type with keyboard on newly paired clients, grant the corresponding client `Mouse Input` and `Keyboard Input` permissions.

## About Virtual Display

> [!WARNING]
> ***It is highly recommend to remove any other virtual display solutions from your system and Apollo/Sunshine config, to reduce confusions and compatibility issues.***

> [!NOTE]
> **TL;DR** Just treat your Artemis/Moonlight client like a dedicated PnP monitor with Apollo.

Apollo uses SudoVDA for virtual display. It features auto resolution and framerate matching for your Artemis/Moonlight clients. The virtual display is created upon the stream starts and removed once the app quits. **If you do not see a new virtual display added or removed when the stream starts or stops, there may be a driver misconfiguration, or another persistent virtual display might still be active.**

The virtual display works just like any physically attached monitors with SudoVDA, there's completely no need for a super complicated solution to "fix" resolution configurations for your devices. Unlike all other solutions that reuses one identity or generate a random one each time for any virtual display sessions, **Apollo assigns a fixed identity for each Artemis/Moonlight client, so your display configuration will be automatically remembered and managed by Windows natively.**

## Configuration for dual GPU laptops

Apollo supports dual GPUs seamlessly.

If you want to use your dGPU, just set the `Adapter Name` to your dGPU and enable `Headless mode` in `Audio/Video` tab, save and restart your computer. No dummy plug is needed any more, the image will be rendered and encoded directly from your dGPU.

## About HDR

HDR starts supporting from Windows 11 23H2 and generally supported on 24H2. Some systems might not have HDR toggle on 23H2 and you just need to upgrade to 24H2. Any system lower than 23H2/Windows 10 will not have HDR option available.

> [!NOTE]
> The below section is written for professional media workers. It doesn't stop you from enabling HDR if you know what you're doing and have deep understanding about how HDR works.
>
> Apollo and SudoVDA can handle HDR just fine like any other streaming solutions.
>
> If you have had good experience with HDR previously, you can safely ignore this section.
>
> If you're curious, read on, but don't blame Apollo for poor HDR support.

Whether HDR streaming looks good, it depends completely on your client.

In short, ICC color correction should be totally useless while streaming HDR. It's your client's job to get HDR content displayed right, not the host. But in fact, it does affect the captured video stream and reflect changes on devices that can handle HDR correctly. On other devices that can't, the info is not respected at all.

It's very complicated to explain why HDR is a total mess, and why enabling HDR makes the image appear dark/yellow. If it's your first time got HDR streaming working, and thinks HDR looks awuful, you're right, but that's not Apollo's fault, it's your device that tone mapped SDR content to the maximum of the capability of its screen, there's no headroom for anything beyond that actual peak brightness for HDR. For details, please take a look [here](https://github.com/ClassicOldSong/Apollo/issues/164).

Usually Apple devices that have HDR capability can be trusted to have good results, other than that, your luck depends.

<details>
<summary>DEPRECATION ALERT</summary>

Enabling HDR is **generally not recommended** with **ANY streaming solutions** at this moment, probably in the long term. The issue with **HDR itself** is huge, with loads of semi-incompatible standards, and massive variance between device configurations and capabilities. Game support for HDR is still choppy.

SDR actually provides much more stable color accuracy, and are widely supported throughout most devices you can imagine. For games, art style can easily overcome the shortcoming with no HDR, and SDR has pretty standard workflows to ensure their visual performance. So HDR isn't *that* important in most of the cases.

</details>

## How to run multiple instances of Apollo for multiple virtual displays

Follow the instructions in the [Wiki](https://github.com/ClassicOldSong/Apollo/wiki/How-to-start-multiple-instances-of-Apollo).

## Stuttering Clinic

Here're some common causes and solutions for stutters. Future updates about stuttering research will be added in [WiKi](https://github.com/ClassicOldSong/Apollo/wiki/Stuttering-Clinic). 

- **Periodic micro stutters**
  + Usually caused by frame rate mismatch, like requested 60fps but the client is only 59.94hz.
  + Checkout [Wiki](https://github.com/ClassicOldSong/Apollo/wiki/Display-Mode-Override) for instructions on how to override the requested frame rate in Apollo.
  + When the frame rate actually matches, you might need to enable VSync or limit frame rate of the game to the same as the requested refresh rate on your host. Client side VSync not that important then, the decoding latency won't go up for VSync if host side and client side matched exactly.
- **Periodic hiccups**
  + Check your monitor/display cable. Some monitors/fiber HDMI cables/HDMI switches probe connections actively, they'll cause the GPU to reinitialize context every time they do so, and stream will be renintialized as well causing a big hiccup.
  + To fix the problem, leave the problematic display open during stream seems always work but it's not ideal.
  + You can also try truning off the problematic display physically, or pull their cable off **from the GPU side**.
  + Some monitors can get the behavior turned off: https://www.reddit.com/r/MoonlightStreaming/comments/1hw5ukh/comment/mjcnlsi
    > One of my monitors has a setting to "Auto Detect Input" and when I turned this off the stuttering is now gone! Even when the monitor is disconnected via software (Windows Display Settings)
- **Random "Slow Network" warnings**
  + Commonly seen on SteamDeck, turn off power saving on the WiFi card seems to be fixing the issue.
  + If still not working, toggle your WiFi connection may fix it temporarily.
- **Stutter when image starts moving, then gets fine**
  + Power saving issue on WiFi cards as well. Typically seen on MTK cards, their default behavior saves power aggressively.
  + Go to Device Manager, find your WiFi adapter, right click, select `Properties`, find `Power Saving` related options and turn them off.
- **Other weird uncatagorized symptons**
  + AMD CPUs paired with Nvidia GPUs sometimes don't work so well. No exact cause found, nor proper solutions to this problem. Workaround is enable "Double refresh rate" mode in `Audio/Video` tab, and limit your game to the requested refresh rate using RTSS or NVCP.
  + [TBD]

## FAQ

- **No virtual display added**
  - Ensure the SudoVDA driver is installed
- **Shows the same screen as main screen**
  - If you're using an external display for the first time, Windows might configure it as "Mirror mode" by default. Press <kbd>Meta + P</kbd> (or known as <kbd>Win + P</kbd>) and select "Extended", then **exit the app** (not only the stream) and start the app again. You only need to do this once.
- **Apps still launch on physical display.**
  - If you don't need to set resolution on your physical display, you can disable the whole advanced config. Then exit Apollo, check if there's a file named "display_device.state", if so remove it, then start Apollo again.

    Then when you stream just disable the physical display in Windows settings. It'll then be managed by Windows.
  - Or you can go to `Audio/Video` tab, in `Advanced display device options` section, set `Device configuration` to `Activate the display automatically and make it a primary display`. Also make sure the app has set preferred display to `None` or `Auto`. This method is not recommended though, as it may fail due to misterious reasons and are sure can mess up your configs.
- **Primary display changed to the virtual display after connection. I don't want that.**
  - Go to `Audio/Video` tab, in `Advanced display device option`s section, set `Devce configuration` to `Disabled` or `Verify that display is enabled` or `Activate the display automatically`. Then go to Windows display settings and set your desired display to primary.
- **I want to turn off the physical monitor when streaming**
  - The first time you stream with virtual display, go to Windows settings and disable the physical monitor. The next time you start streaming it will turn off automatically.
  - Or, go to `Audio/Video` tab, in `Advanced display device option`s section, set `Devce configuration` to `Deactivate other displays and activate only the specified display`.
- **I want all apps to start in virtual display**
  - Simply enable `Headless Mode` in `Audio/Video` tab. Make sure you have set the encoder capability manually in `Advanced` tab.
- **Client says Host doesn't support HDR/444 but it actually does**
  - This only happens when you enabled `Headless Mode`. The initial capability probing is skipped during startup and is performed on the first connection.
  - If you don't mind, quit and enter the stream again should fix the problem, or just go to `Advanced` tab and set the advertised capability to `Always advertise` based on the actual capability of your GPU in use.
- **Virtual Display entry does not run any commands**
  - The Virtual Display entry also acts as a safe mode entry, so all commands and customizations are disabled for it.
  - If you want specific app to use virtual display, just enable `Always use Virtual Display` option for it.
- **HDR isn't enabled when using battery**
  - Check out [To play HDR content when running on battery](https://support.microsoft.com/en-us/windows/hdr-settings-in-windows-2d767185-38ec-7fdc-6f97-bbc6c5ef24e6)
  - [Archive](https://web.archive.org/web/20240828044038/https://support.microsoft.com/en-us/windows/hdr-settings-in-windows-2d767185-38ec-7fdc-6f97-bbc6c5ef24e6) to the above link in case M$ remove it unexpectedly someday
- **Resolution can't match client side request anymore**
  - ***NEVER*** set screen rotation on virtual displays! Apollo can handle vertical display normally, there's no need to manually set screen rotation if you're using [Artemis](https://github.com/ClassicOldSong/moonlight-android) with Apollo.
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
- **I would like to capture sound from only one app.**
  - Check out the [Wiki](https://github.com/ClassicOldSong/Apollo/wiki/Stream-audio-from-only-one-app)

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
| OS            | Windows: 10+ (Windows Server requires [manual installation](https://github.com/nefarius/ViGEmBus/issues/153) for gamepad support) |
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

Currently support is only provided via GitHub Issues/Discussions.

No real time chat support will ever be provided for Apollo and Artemis. Including but not limited to:

- Discord
- Telegram
- Whatsapp
- QQ
- WeChat 

> When there's a chat, there're dramas. -- Confucius

## Downloads

[Releases](https://github.com/ClassicOldSong/Apollo/releases)

## Disclaimer

I got kicked from Moonlight and Sunshine's Discord server and banned from Sunshine's GitHub repo literally for helping people out.

This is what I got for finding a bug, opened an issue, getting no response, troubleshoot myself, fixed the issue myself, shared it by PR to the main repo hoping my efforts can help someone else during the maintenance gap.

Yes, I'm going away. [Apollo](https://github.com/ClassicOldSong/Apollo) and [Artemis(Moonlight Noir)](https://github.com/ClassicOldSong/moonlight-android) will no longer be compatible with OG Sunshine and OG Moonlight eventually, but they'll work even better with much more carefully designed features.

The Moonlight repo had stayed silent for 5 months, with nobody actually responding to issues, and people are getting totally no help besides the limited FAQ in their Discord server. I tried to answer issues and questions, solve problems within my ability but I got kicked out just for helping others.

**PRs for feature improvements are welcomed here unlike the main repo, your ideas are more likely to be appreciated and your efforts are actually being respected. We welcome people who can and willing to share their efforts, helping yourselves and other people in need.**

**Update**: They have contacted me and apologized for this incident, but the fact it **happened** still motivated me to start my own fork.

## License

GPLv3
