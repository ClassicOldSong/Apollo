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
- [DRAW TO HOME 3D](https://studyquests.github.io/draw-to-home-3d.html)
- [ZOMBIE CHASE](https://quizverses.pages.dev/zombie-chase.html)
- [U SHAPE PUZZLE](https://quizverses.github.io/u-shape-puzzle.html)
- [WORLD CUP SOCCER CAPS](https://quizverses.github.io/world-cup-soccer-caps.html)
- [WEDNESDAY ADDAMS BEAUTY SALON](https://quizverses.github.io/wednesday-addams-beauty-salon.html)
- [HOOK MASTER MAFIA CITY](https://quizverses.pages.dev/hook-master-mafia-city.html)
- [ASOKA MAKEUP INDIAN BRIDE](https://quizverses.pages.dev/asoka-makeup-indian-bride.html)
- [ROBBIE BECOME A BEAST](https://quizverses.pages.dev/robbie-become-a-beast.html)
- [PRINCESS ROYAL WEDDING](https://quizverses.github.io/princess-royal-wedding.html)
- [BRUTALMANIA IO](https://quizverses.github.io/brutalmania-io.html)
- [DIRTY MONEY THE RICH GET RICH](https://quizverses.github.io/dirty-money-the-rich-get-rich.html)
- [GROW WARSIO](https://quizverses-9d2f2.web.app/grow-warsio.html)
- [SNAKE HUNTER](https://quizverses.pages.dev/snake-hunter.html)
- [NUBIK COURIER AN OPEN WORLD](https://quizverses.pages.dev/nubik-courier-an-open-world.html)
- [ARROW SLIDE PUZZLE](https://quizverses.pages.dev/arrow-slide-puzzle.html)
- [MAHJONG QUEST CANDYLAND ADVENTURES](https://studyplayings.pages.dev/mahjong-quest-candyland-adventures.html)
- [STICKMAN ARCHER SHOOTING ARROWS AT REDS](https://quizverses.pages.dev/stickman-archer-shooting-arrows-at-reds.html)
- [NINJA CLIMB](https://studyquests.github.io/ninja-climb.html)
- [SQUID GAME PLAYGROUND SHOOTER](https://quizverses.pages.dev/squid-game-playground-shooter.html)
- [TUNG TUNG SAHUR COLORING BOOK](https://studyquests.pages.dev/tung-tung-sahur-coloring-book.html)
- [HIGH SPEED CRAZY BIKE](https://studyquests.github.io/high-speed-crazy-bike.html)
- [CATEGORY PUZZLE 3](https://studyplayings.web.app/category-puzzle-3.html)
- [STACK UP](https://studyplayings.pages.dev/stack-up.html)
- [OBBY DEAD RIVER](https://studyquests.pages.dev/obby-dead-river.html)
- [THE EARTH EVOLUTION](https://studyplayings.web.app/the-earth-evolution.html)
- [HUMAN EVOLUTION RUN](https://quizverses.github.io/human-evolution-run.html)
- [IDLE TRADE ISLE](https://quizverses.github.io/idle-trade-isle.html)
- [NUTS BOLTS WOOD PUZZLE GAME](https://studyplayings.web.app/nuts-bolts-wood-puzzle-game.html)
- [CATEGORY BOOKMARKLETS](https://studyplayings.pages.dev/category-bookmarklets.html)
- [CATEGORY GROW99](https://studyplayings.web.app/category-grow99.html)
- [TRAFFIC ESCAPE PUZZLE](https://studyplayings.pages.dev/traffic-escape-puzzle.html)
- [INDEX10](https://quizverses.github.io/index10.html)
- [BATTLEDUDES IO](https://studyquests.github.io/battledudes-io.html)
- [CATEGORY WAR](https://studyquesthub.web.app/category-war.html)
- [FIGHT TO THE END](https://quizverses.pages.dev/fight-to-the-end.html)
- [FRAGEN](https://quizverses.pages.dev/fragen.html)
- [PRIVACY](https://brainquests.onrender.com/privacy.html)
- [BONNIE FITNESS FRENZY](https://quizverses.pages.dev/bonnie-fitness-frenzy.html)
- [FIGHT FOR THE TREE](https://studyplaying.github.io/fight-for-the-tree.html)
- [ACE CAR RACING](https://studyplayings.pages.dev/ace-car-racing.html)
- [BULLET HEROES](https://quizverses.pages.dev/bullet-heroes.html)
- [COLOR DOTS CHALLENGE](https://studyplayings.pages.dev/color-dots-challenge.html)
- [UNCLE BULLET 007](https://quizverses.pages.dev/uncle-bullet-007.html)
- [BALL PAINT 3D](https://quizverses.github.io/ball-paint-3d.html)
- [ARMY TRUCK DRIVER ONLINE](https://studyplaying.github.io/army-truck-driver-online.html)
- [CIRCLE SHOOTER MASTER](https://quizverses.github.io/circle-shooter-master.html)
- [INDEX8](https://studyquesthub.web.app/index8.html)
- [CATEGORY SIDE SCROLLING184](https://studyplaying.github.io/category-side-scrolling184.html)
- [CATEGORY STICKMAN175](https://quizverses-9d2f2.web.app/category-stickman175.html)
- [HALLOWEEN STICKMAN](https://studyplaying.github.io/halloween-stickman.html)
- [WONDERS OF EGYPT MATCH](https://quizverses.pages.dev/wonders-of-egypt-match.html)
- [CATEGORY 2D1 060](https://studyplayings.web.app/category-2d1-060.html)
- [SLICER DUO](https://studyplaying.github.io/slicer-duo.html)
- [DRIVER MASTER SIMULATOR](https://quizverses.github.io/driver-master-simulator.html)
- [CATEGORY HUNTING16](https://quizverses.github.io/category-hunting16.html)
- [CATEGORY TOOLS](https://studyplayings.pages.dev/category-tools.html)
- [MY DINOSAUR LAND](https://studyplayings.pages.dev/my-dinosaur-land.html)
- [PAPERLY PAPER PLANE ADVENTURE](https://studyplaying.github.io/paperly-paper-plane-adventure.html)
- [CATEGORY FASHION](https://studyplayings.web.app/category-fashion.html)
- [FOOTBALL FUN](https://thelearnquesters.pages.dev/football-fun.html)
- [CRAZY BUNNIES](https://quizverses.github.io/crazy-bunnies.html)
- [CATEGORY INCREMENTAL388](https://thelearnquester.web.app/category-incremental388.html)
- [DEAD ZONE MECH OPS](https://studyplaying.github.io/dead-zone-mech-ops.html)
- [2248 MUSICAL](https://quizverses.github.io/2248-musical.html)
- [CATEGORY FLASH](https://quizverses.github.io/category-flash.html)
- [MATH WALL SIMULATOR](https://studyplaying.github.io/math-wall-simulator.html)
- [CATEGORY ADVENTURE 4](https://studyquests.github.io/category-adventure-4.html)
- [PRIVACY](https://studyquesthub.web.app/privacy.html)
- [CATEGORY ESCAPE 2](https://studyplayings.web.app/category-escape-2.html)
- [STICKMAN FIGHT PRO](https://studyplaying.github.io/stickman-fight-pro.html)
- [OBBY PARKOUR RACING](https://quizverses-9d2f2.web.app/obby-parkour-racing.html)
- [SERIOUS BRO](https://quizverses.github.io/serious-bro.html)
- [NO PAIN NO GAIN RAGDOLL SANDBOX](https://quizverses.pages.dev/no-pain-no-gain-ragdoll-sandbox.html)
- [IDLE LANDMARK BUILDER](https://studyplaying.github.io/idle-landmark-builder.html)
- [BALL TOWER OF HELL](https://quizverses.github.io/ball-tower-of-hell.html)
- [JIGSAW M](https://quizverses.pages.dev/jigsaw-m.html)
- [CATEGORY CARDS](https://studyplayings.web.app/category-cards.html)
- [CATEGORY BRAIN261](https://learnquester.github.io/category-brain261.html)
- [CATEGORY ADVENTURE 2](https://quizverses-9d2f2.web.app/category-adventure-2.html)
- [RUNNING IN FOAM](https://studyplaying.github.io/running-in-foam.html)
- [FASHIONISTA CHRISTMAS EVE PARTY](https://quizverses.pages.dev/fashionista-christmas-eve-party.html)
- [CUBE KING](https://studyquests.pages.dev/cube-king.html)
- [BUBBLE SHOOTER PANDA BLAST](https://studyplaying.github.io/bubble-shooter-panda-blast.html)
- [OBBY GYM SIMULATOR ESCAPE](https://studyquesthub.web.app/obby-gym-simulator-escape.html)
- [FORTUNES DECK SOLITAIRE](https://studyplayings.pages.dev/fortunes-deck-solitaire.html)
- [MEDIEVAL ESCAPE](https://studyplayings.pages.dev/medieval-escape.html)
- [CATEGORY GROW99](https://quizverses-9d2f2.web.app/category-grow99.html)
- [MOJICON EMOJI CONNECT](https://studyplaying.github.io/mojicon-emoji-connect.html)
- [FAMILY TREE PUZZLE](https://studyquests.github.io/family-tree-puzzle.html)
- [TWO DOTS REMASTERED](https://studyquests.pages.dev/two-dots-remastered.html)
- [DOGGI](https://studyquesthub.web.app/doggi.html)
- [STAR ATTACK 3D](https://studyquesthub.web.app/star-attack-3d.html)
- [CARNAGE BATTLE ARENA](https://studyplayings.pages.dev/carnage-battle-arena.html)
- [FURY ROAD ZOMBIE CRASH](https://studyplayings.pages.dev/fury-road-zombie-crash.html)
- [CATEGORY BASKETBALL](https://thelearnquester.web.app/category-basketball.html)
- [BOUNCEPOP QUEST](https://studyplayings.pages.dev/bouncepop-quest.html)
- [MINETAP MERGE CLICKER](https://studyquesthub.web.app/minetap-merge-clicker.html)
- [CATEGORY SOLITAIRE27](https://thelearnquester.web.app/category-solitaire27.html)
- [AVATAR LIFE MY TOWN](https://quizverses.pages.dev/avatar-life-my-town.html)
- [CATEGORY BIKE](https://studyquests.github.io/category-bike.html)
- [CATEGORY CAR](https://quizverses-9d2f2.web.app/category-car.html)
- [ROOM SORT FLOOR PLAN](https://studyquests.pages.dev/room-sort-floor-plan.html)
- [OBBY PRISON CRAFT ESCAPE](https://studyplayings.web.app/obby-prison-craft-escape.html)
- [VEGAMIX DA VINCI PUZZLES](https://studyplayings.web.app/vegamix-da-vinci-puzzles.html)
- [OVERFLOWING PALETTE](https://quizverses.github.io/overflowing-palette.html)
- [GEOMETRY LITE](https://quizverses.github.io/geometry-lite.html)
- [THE BIG HIT RUN](https://studyquests.github.io/the-big-hit-run.html)
- [DOGS VS ALIENS](https://studyquests.github.io/dogs-vs-aliens.html)
- [CLINIC CLEANUP CREW](https://studyplayings.web.app/clinic-cleanup-crew.html)
- [HOME RUSH THE FISH WAR](https://studyquests.pages.dev/home-rush-the-fish-war.html)
- [CATEGORY PUZZLE 5](https://quizverses-9d2f2.web.app/category-puzzle-5.html)
- [IDLE BARBER SHOP](https://quizverses.pages.dev/idle-barber-shop.html)
- [GETTING OVER IT](https://quizverses-9d2f2.web.app/getting-over-it.html)
- [COLOR SAND PUZZLE](https://themindzone.pages.dev/color-sand-puzzle.html)
- [COIN BLITZ](https://thelearnquesters.pages.dev/coin-blitz.html)
- [CATEGORY BATTLE523](https://studyquests.github.io/category-battle523.html)
- [CIRCLE SHOOTER MASTER](https://thelearnquesters.pages.dev/circle-shooter-master.html)
- [GROW WARSIO](https://thelearnquesters.pages.dev/grow-warsio.html)
- [URBAN ASSAULT FORCE](https://thelearnquesters.pages.dev/urban-assault-force.html)
- [MAHJONG CONNECT SPOOKY](https://studyquests.pages.dev/mahjong-connect-spooky.html)
- [CATEGORY BATTLE524](https://quizverses-9d2f2.web.app/category-battle524.html)
- [LABUBU AND TREASURES FUN ADVENTURE](https://quizverses.github.io/labubu-and-treasures-fun-adventure.html)
- [KIRKA IO](https://thelearnquesters.pages.dev/kirka-io.html)
- [HIDDEN OBJECT STREET OF SECRETS](https://studyplayings.pages.dev/hidden-object-street-of-secrets.html)
- [SCREW PUZZLE](https://thelearnquesters.pages.dev/screw-puzzle.html)
- [LOVIE CHICS COACHELLA FESTIVAL](https://studyquesthub.web.app/lovie-chics-coachella-festival.html)
- [NONOGRAM MASTER](https://thelearnquesters.pages.dev/nonogram-master.html)
- [INDEX15](https://thelearnquester.web.app/index15.html)
- [BLOCK PUZZLE SLIDE BLOCK JAM](https://quizverses-9d2f2.web.app/block-puzzle-slide-block-jam.html)
- [BOARD KINGS BOARD DICE](https://studyplaying.github.io/board-kings-board-dice.html)
