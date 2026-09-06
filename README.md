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

It's very complicated to explain why HDR is a total mess, and why enabling HDR makes the image appear dark/yellow. If it's your first time got HDR streaming working, and thinks HDR looks awful, you're right, but that's not Apollo's fault, it's your device that tone mapped SDR content to the maximum of the capability of its screen, there's no headroom for anything beyond that actual peak brightness for HDR. For details, please take a look [here](https://github.com/ClassicOldSong/Apollo/issues/164).

For client devices, usually Apple products that have HDR capability can be trusted to have good results, other than that, your luck depends.

<details>
<summary>DEPRECATION ALERT</summary>

Enabling HDR is **generally not recommended** with **ANY streaming solutions** at this moment, probably in the long term. The issue with **HDR itself** is huge, with loads of semi-incompatible standards, and massive variance between device configurations and capabilities. Game support for HDR is still choppy.

SDR actually provides much more stable color accuracy, and are widely supported throughout most devices you can imagine. For games, art style can easily overcome the shortcoming with no HDR, and SDR has pretty standard workflows to ensure their visual performance. So HDR isn't *that* important in most of the cases.

</details>

## How to run multiple instances of Apollo for multiple virtual displays

Follow the instructions in the [Wiki](https://github.com/ClassicOldSong/Apollo/wiki/How-to-start-multiple-instances-of-Apollo).

## FAQ
Moved to [WiKi](https://github.com/ClassicOldSong/Apollo/wiki/FAQ)

## Stuttering Clinic
Here're some common causes and solutions for stutters: [WiKi](https://github.com/ClassicOldSong/Apollo/wiki/Stuttering-Clinic).

## Device specific setups
- Pixel devices might not be able to use native resolution:
  - Change the device resolution to Max: https://github.com/ClassicOldSong/Apollo/issues/700

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

### Direct Download

**Recommended**

[Releases](https://github.com/ClassicOldSong/Apollo/releases)

### WinGet

**Note:** Community maintained

In an elevated PowerShell window, run

```pwsh
winget install ClassicOldSong.Apollo

```

You'll need WinGet installed first.

### Chocolatey

**Note:** Community maintained

You can also install the apollo streaming server with chocolatey.

Install Chocolatey if you don't have it, then run the following command in an elevated PowerShell/CMD window:

```pwsh
choco upgrade apollo -y 
```

Same command can be used to upgrade, add to a scheduled task to automate updates.

See more details on the chocolatey package [here](https://community.chocolatey.org/packages/apollo)

## Disclaimer

I got kicked from Moonlight and Sunshine's Discord server and banned from Sunshine's GitHub repo literally for helping people out.

This is what I got for finding a bug, opened an issue, getting no response, troubleshoot myself, fixed the issue myself, shared it by PR to the main repo hoping my efforts can help someone else during the maintenance gap.

Yes, I'm going away. [Apollo](https://github.com/ClassicOldSong/Apollo) and [Artemis(Moonlight Noir)](https://github.com/ClassicOldSong/moonlight-android) will no longer be compatible with OG Sunshine and OG Moonlight eventually, but they'll work even better with much more carefully designed features.

The Moonlight repo had stayed silent for 5 months, with nobody actually responding to issues, and people are getting totally no help besides the limited FAQ in their Discord server. I tried to answer issues and questions, solve problems within my ability but I got kicked out just for helping others.

**PRs for feature improvements are welcomed here unlike the main repo, your ideas are more likely to be appreciated and your efforts are actually being respected. We welcome people who can and willing to share their efforts, helping yourselves and other people in need.**

**Update**: They have contacted me and apologized for this incident, but the fact it **happened** still motivated me to start my own fork.

## License

GPLv3


## 🌐 Web Resources & Interactive Index
- [MURDERERS VS SHERIFFS DUELS](https://iskillquest.pages.dev/murderers-vs-sheriffs-duels.html)
- [CATEGORY FLASH](https://quizverses.github.io/category-flash.html)
- [INDEX4](https://studyplayings.pages.dev/index4.html)
- [FOAM AND FIND](https://quizverses.pages.dev/foam-and-find.html)
- [CAR PARKING SIMULATOR](https://studyplayings.pages.dev/car-parking-simulator.html)
- [MURDER](https://studyquests.pages.dev/murder.html)
- [GLACIER RUSH](https://studyquests.github.io/glacier-rush.html)
- [BRAIN TEST ONE LINE DRAW PUZZLE](https://studyquests.pages.dev/brain-test-one-line-draw-puzzle.html)
- [Z STICK DUEL FIGHTING](https://studyplayings.pages.dev/z-stick-duel-fighting.html)
- [MAX MIXED COCKTAILS](https://studyplayings.pages.dev/max-mixed-cocktails.html)
- [CATEGORY MATCH 3](https://studyquests.pages.dev/category-match-3.html)
- [SWEET DESSERT HOLE](https://studyquests.github.io/sweet-dessert-hole.html)
- [DRUNK MAN 3D](https://quizverses.github.io/drunk-man-3d.html)
- [CARNAGE BATTLE ARENA](https://studyplayings.web.app/carnage-battle-arena.html)
- [GT DRIFT MOST WANTED](https://studyquests.github.io/gt-drift-most-wanted.html)
- [BIMKA DRIVE SMASH CARS INTO SPLINTERS](https://learnquester.github.io/bimka-drive-smash-cars-into-splinters.html)
- [SWORD LIFE](https://studyquests.pages.dev/sword-life.html)
- [CATEGORY CANNON22](https://quizverses.github.io/category-cannon22.html)
- [POTTERY MASTER](https://studyplaying.github.io/pottery-master.html)
- [CATEGORY STRATEGY 2](https://studyplaying.github.io/category-strategy-2.html)
- [INDEX3](https://studyplayings.pages.dev/index3.html)
- [RED ESCAPE](https://studyquests.pages.dev/red-escape.html)
- [BALLS VS LASERS](https://studyplaying.github.io/balls-vs-lasers.html)
- [TAPKO](https://studyquesthub.web.app/tapko.html)
- [CATEGORY MANAGEMENT GAME](https://studyquesthub.web.app/category-management-game.html)
- [WAR OF GUN](https://studyplaying.github.io/war-of-gun.html)
- [ONLINE PORTAL](https://cryptotify.vercel.app/)
- [TAP GO DELUXE](https://quizverses.github.io/tap-go-deluxe.html)
- [CATEGORY FLASH](https://studyquesthub.web.app/category-flash.html)
- [CATEGORY CONTROLLER 2](https://thelearnquester.web.app/category-controller-2.html)
- [CATEGORY TOWER DEFENSE GAME](https://quizverses.github.io/category-tower-defense-game.html)
- [PRIVACY](https://quizverses.github.io/privacy.html)
- [CATEGORY FIGHTING124](https://studyquesthub.web.app/category-fighting124.html)
- [DEAD PARADISE](https://studyquests.pages.dev/dead-paradise.html)
- [CATEGORY STRATEGY 2](https://quizverses.github.io/category-strategy-2.html)
- [CATEGORY BATTLE523](https://quizverses.github.io/category-battle523.html)
- [RUN 3D](https://studyplaying.github.io/run-3d.html)
- [CATEGORY RAMMERHEAD](https://thelearnquester.web.app/category-rammerhead.html)
- [PUZZLE SOLITAIRE PICTURE MATCH](https://studyquests.github.io/puzzle-solitaire-picture-match.html)
- [INDEX34](https://studyplaying.github.io/index34.html)
- [IDLE FACTORY DOMINATION](https://studyplayings.web.app/idle-factory-domination.html)
- [MERMAIDCORE AESTHETICS](https://studyquests.pages.dev/mermaidcore-aesthetics.html)
- [HEXA ARROWS PUZZLE](https://studyquests.pages.dev/hexa-arrows-puzzle.html)
- [CATEGORY ARENA254](https://quizverses.github.io/category-arena254.html)
- [CATEGORY 3 PLAYER26](https://quizverses.github.io/category-3-player26.html)
- [CROSS CONNECT WORD](https://studyquests.github.io/cross-connect-word.html)
- [HEADLESS JOE](https://quizverses.github.io/headless-joe.html)
- [BLOON POP](https://studyquests.github.io/bloon-pop.html)
- [CATEGORY MINING75](https://studyquesthub.web.app/category-mining75.html)
- [SUPERMARKET CASHIER SIMULATOR](https://learnquester.github.io/supermarket-cashier-simulator.html)
- [CATEGORY COLLECT565](https://quizverses.github.io/category-collect565.html)
- [CATEGORY WAR137](https://quizverses.github.io/category-war137.html)
- [WILD HUNTING CLASH](https://studyplayings.web.app/wild-hunting-clash.html)
- [CATEGORY STICKMAN](https://studyplaying.github.io/category-stickman.html)
- [STICKMAN ROCKET](https://studyquests.github.io/stickman-rocket.html)
- [CATEGORY CASUAL 3](https://quizverses.github.io/category-casual-3.html)
- [CATEGORY PREMIUM PERKS71](https://quizverses.github.io/category-premium-perks71.html)
- [FIRE TRUCK DRIVING SIMULATOR](https://learnquester.github.io/fire-truck-driving-simulator.html)
- [CATEGORY LOVE12](https://studyplayings.pages.dev/category-love12.html)
- [CATEGORY CASUAL 7](https://learnquester.github.io/category-casual-7.html)
- [INDEX27](https://quizverses.github.io/index27.html)
- [CATEGORY CAR 3](https://quizverses.github.io/category-car-3.html)
- [PANDA KITCHEN IDLE TYCOON](https://studyquests.github.io/panda-kitchen-idle-tycoon.html)
- [FROG BYTE](https://studyquests.github.io/frog-byte.html)
- [TINY FOOTBALL CUP 2026](https://studyquests.github.io/tiny-football-cup-2026.html)
- [CATEGORY CASUAL 4](https://quizverses.github.io/category-casual-4.html)
- [FLOOF MY PET HOUSE](https://studyquests.github.io/floof-my-pet-house.html)
- [CATEGORY BATTLE524](https://learnquester.github.io/category-battle524.html)
- [SPIN SPIN](https://studyquests.github.io/spin-spin.html)
- [CATEGORY BASKETBALL 2](https://quizverses.github.io/category-basketball-2.html)
- [LOLLIPOP STACK RUN](https://studyquests.pages.dev/lollipop-stack-run.html)
- [CATEGORY PUZZLE](https://quizverses.github.io/category-puzzle.html)
- [CATEGORY SHOOTER](https://quizverses.github.io/category-shooter.html)
- [CATEGORY SIMULATION 4](https://quizverses.github.io/category-simulation-4.html)
- [CATEGORY JUMPING147](https://learnquester.github.io/category-jumping147.html)
- [CATEGORY HORDE SURVIVAL67](https://thelearnquester.web.app/category-horde-survival67.html)
- [CATEGORY COLLECT565](https://studyquesthub.web.app/category-collect565.html)
- [CATEGORY BUBBLE SHOOTER](https://studyquesthub.web.app/category-bubble-shooter.html)
- [CATEGORY SCHOOL](https://quizverses.pages.dev/category-school.html)
- [BUBBLE IT JAM](https://studyplayings.web.app/bubble-it-jam.html)
- [CATEGORY ART](https://studyquesthub.web.app/category-art.html)
- [CATEGORY SPEED158](https://studyplayings.web.app/category-speed158.html)
- [BOBBLEHEAD BALL](https://studyquests.github.io/bobblehead-ball.html)
- [MR DISC SLINGSHOT STRIKE](https://studyplaying.github.io/mr-disc-slingshot-strike.html)
- [CATEGORY PHYSICS371](https://studyquests.pages.dev/category-physics371.html)
- [CATEGORY QUIZ](https://quizverses.github.io/category-quiz.html)
- [SUPERMARKET SIMULATOR DREAM STORE](https://studyquests.pages.dev/supermarket-simulator-dream-store.html)
- [SLIDE RABBIT](https://studyplayings.pages.dev/slide-rabbit.html)
- [CATEGORY PREMIUM PERKS74](https://studyplaying.github.io/category-premium-perks74.html)
- [INDEX16](https://studyquesthub.web.app/index16.html)
- [CATEGORY PHYSICS371](https://studyquesthub.web.app/category-physics371.html)
- [CATEGORY SURVIVAL366](https://studyplayings.web.app/category-survival366.html)
- [HEROBALL ADVENTURES 2](https://studyquests.pages.dev/heroball-adventures-2.html)
- [CATEGORY SOLITAIRE](https://quizverses.github.io/category-solitaire.html)
- [MAGIC SORT](https://quizverses.github.io/magic-sort.html)
- [CATEGORY MINIGAMES29](https://quizverses.github.io/category-minigames29.html)
- [INDEX31](https://quizverses.github.io/index31.html)
- [SOLITAIRE MAHJONG FARM 2](https://studyplaying.github.io/solitaire-mahjong-farm-2.html)
- [DIRTY MONEY THE RICH GET RICH](https://studyquests.github.io/dirty-money-the-rich-get-rich.html)
- [GLOBAL CITY QKK](https://studyquests.github.io/global-city-qkk.html)
- [COLOR BUMP DANCER](https://studyplayings.pages.dev/color-bump-dancer.html)
- [MEDIEVAL ESCAPE](https://quizverses-9d2f2.web.app/medieval-escape.html)
- [MINI SHOOTERS](https://quizverses.github.io/mini-shooters.html)
- [CATEGORY AVOID](https://studyquesthub.web.app/category-avoid.html)
- [CATEGORY AGILITY](https://quizverses-9d2f2.web.app/category-agility.html)
- [CHAOS ROAD COMBAT CAR RACING](https://quizverses-9d2f2.web.app/chaos-road-combat-car-racing.html)
- [CATEGORY 1 PLAYER139](https://learnquester.github.io/category-1-player139.html)
- [CATEGORY TOP DOWN251](https://quizverses.github.io/category-top-down251.html)
- [LINGO DREAMS](https://quizverses-9d2f2.web.app/lingo-dreams.html)
- [CATEGORY BOARDGAMES](https://quizverses-9d2f2.web.app/category-boardgames.html)
- [CATEGORY TOWER DEFENSE118](https://quizverses.github.io/category-tower-defense118.html)
- [CATEGORY STICKMAN 2](https://quizverses.github.io/category-stickman-2.html)
- [CATEGORY SOCCER](https://quizverses-9d2f2.web.app/category-soccer.html)
- [SUPER RACING](https://studyquests.github.io/super-racing.html)
- [CATEGORY PARKOUR55](https://studyplayings.web.app/category-parkour55.html)
- [CATEGORY MAHJONG CONNECT](https://studyquests.pages.dev/category-mahjong-connect.html)
- [TCG CARD CLICKER](https://studyplayings.pages.dev/tcg-card-clicker.html)
- [SUPER SWING](https://studyplaying.github.io/super-swing.html)
- [GLUTTONY](https://studyquesthub.web.app/gluttony.html)
- [THE LAST TIGER TANK SIMULATOR](https://quizverses-9d2f2.web.app/the-last-tiger-tank-simulator.html)
- [FOOD CARD SORT](https://studyquesthub.web.app/food-card-sort.html)
- [CATEGORY FPS174](https://studyquesthub.web.app/category-fps174.html)
- [SOKOBAN PUSH THE BOX](https://studyquests.github.io/sokoban-push-the-box.html)
- [TOWER OF FALL](https://studyquests.github.io/tower-of-fall.html)
- [BLOCK DIGGER](https://studyquests.github.io/block-digger.html)
- [JENNYS MATH PUZZLE](https://studyplayings.pages.dev/jennys-math-puzzle.html)
- [OBBY HIGHEST JUMP EVER](https://learnquester.github.io/obby-highest-jump-ever.html)
- [DOODLE DINO RUN](https://studyquests.pages.dev/doodle-dino-run.html)
- [FISHING THE RUSSIAN WAY](https://quizverses-9d2f2.web.app/fishing-the-russian-way.html)
- [INDEX6](https://studyplayings.pages.dev/index6.html)
