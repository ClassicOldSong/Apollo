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
- [STAND ON THE RIGHT COLOR ROBBY](https://studyquesthub.web.app/stand-on-the-right-color-robby.html)
- [BATTLE ARENA RACE TO WIN](https://studyquesthub.web.app/battle-arena-race-to-win.html)
- [CATEGORY HERO72](https://thelearnquester.web.app/category-hero72.html)
- [KNIFE MASTER BALL RACING](https://themindzone.pages.dev/knife-master-ball-racing.html)
- [TWILIGHT SOLITAIRE TRIPEAKS](https://quizverses-9d2f2.web.app/twilight-solitaire-tripeaks.html)
- [PET SALON SIMULATOR](https://quizverses.pages.dev/pet-salon-simulator.html)
- [CATEGORY CASUAL969](https://studyplayings.pages.dev/category-casual969.html)
- [CATEGORY CASUAL971](https://quizverses-9d2f2.web.app/category-casual971.html)
- [COLOR CLASH](https://studyquests.pages.dev/color-clash.html)
- [OFFROAD JEEP GAME SIMULATOR](https://learnquester.github.io/offroad-jeep-game-simulator.html)
- [CUPIDS STORY LOVE ARCHER BOW](https://quizverses.pages.dev/cupids-story-love-archer-bow.html)
- [CATEGORY RPG GAMES](https://studyquesthub.web.app/category-rpg-games.html)
- [RUN 3D](https://studyquests.github.io/run-3d.html)
- [CATEGORY PUZZLE 5](https://quizverses.github.io/category-puzzle-5.html)
- [GEOMETRY MISSILE](https://studyquests.pages.dev/geometry-missile.html)
- [UPHILL RUSH 13](https://studyquests.pages.dev/uphill-rush-13.html)
- [SHEEP SHEEP DUCK](https://quizverses-9d2f2.web.app/sheep-sheep-duck.html)
- [SNIPING ALIENS](https://studyplayings.pages.dev/sniping-aliens.html)
- [TAP GO DELUXE](https://quizverses.pages.dev/tap-go-deluxe.html)
- [HUNGRY NOOB CAFE SIMULATOR](https://learnquester.github.io/hungry-noob-cafe-simulator.html)
- [OBBY GYM SIMULATOR ESCAPE](https://quizverses.pages.dev/obby-gym-simulator-escape.html)
- [CATEGORY DEFENSE176](https://studyquesthub.web.app/category-defense176.html)
- [CATEGORY SHOOTER](https://studyplayings.pages.dev/category-shooter.html)
- [CATEGORY FASHION105](https://studyquests.pages.dev/category-fashion105.html)
- [CATEGORY CASUAL 2](https://quizverses-9d2f2.web.app/category-casual-2.html)
- [OLE BUNNY](https://studyquests.github.io/ole-bunny.html)
- [TRIANGLE WAY](https://quizverses.pages.dev/triangle-way.html)
- [BRAINROT MOB CLASH 3D](https://studyquests.github.io/brainrot-mob-clash-3d.html)
- [ROLLING BALLS SEA RACE](https://learnquester.github.io/rolling-balls-sea-race.html)
- [MONSTER ARENA](https://quizverses.github.io/monster-arena.html)
- [HIDDEN KITTY](https://quizverses.github.io/hidden-kitty.html)
- [MUSIC TILES FLUFFY HOP BEAT](https://studyplayings.pages.dev/music-tiles-fluffy-hop-beat.html)
- [MR LONG LEGS](https://studyquesthub.web.app/mr-long-legs.html)
- [BASE JUMP WINGSUIT FLYING](https://studyquests.github.io/base-jump-wingsuit-flying.html)
- [SITEMAP](https://quizverses-9d2f2.web.app/sitemap.html)
- [WORLDCRAFT 3](https://studyplayings.pages.dev/worldcraft-3.html)
- [MY FARM LIFE](https://studyplayings.pages.dev/my-farm-life.html)
- [INDEX19](https://thelearnquester.web.app/index19.html)
- [TERMS](https://cryptotify.pages.dev/terms.html)
- [PRIVACY](https://brainquests.vercel.app/privacy.html)
- [ITALIAN BRAINROT IN GEOMETRY DASH](https://studyquesthub.web.app/italian-brainrot-in-geometry-dash.html)
- [CATEGORY TOWER DEFENSE 3](https://quizverses.github.io/category-tower-defense-3.html)
- [CATEGORY INCREMENTAL388](https://quizverses-9d2f2.web.app/category-incremental388.html)
- [WORM PUZZLE SNAKE APPLE](https://learnquester.github.io/worm-puzzle-snake-apple.html)
- [MONSTER DUELIST](https://studyquesthub.web.app/monster-duelist.html)
- [MUSHROOM FEVER MATCH 3](https://quizverses-9d2f2.web.app/mushroom-fever-match-3.html)
- [MR CAPPUCCINO ASSASSINO](https://learnquester.github.io/mr-cappuccino-assassino.html)
- [ZOMBIE RAFT](https://studyquests.pages.dev/zombie-raft.html)
- [DRAW CLIMBER](https://studyquesthub.web.app/draw-climber.html)
- [OIL DIGGING](https://studyplayings.pages.dev/oil-digging.html)
- [CATEGORY CASUAL 7](https://thelearnquester.web.app/category-casual-7.html)
- [INDEX17](https://quizverses-9d2f2.web.app/index17.html)
- [CATEGORY CUTE](https://learnquester.github.io/category-cute.html)
- [CLICKER KNIGHTS VS DRAGONS](https://quizverses.github.io/clicker-knights-vs-dragons.html)
- [PRINCESSES OF QUADROBICS](https://studyquests.github.io/princesses-of-quadrobics.html)
- [CATEGORY PUZZLE](https://quizverses-9d2f2.web.app/category-puzzle.html)
- [GIRLFRIEND FROM HELL](https://quizverses-9d2f2.web.app/girlfriend-from-hell.html)
- [TERMS](https://studyquests.pages.dev/terms.html)
- [IDLE MONEY FACTORY](https://learnquester.github.io/idle-money-factory.html)
- [VALENTINES HIDDEN ALPHAWORDS](https://studyplayings.pages.dev/valentines-hidden-alphawords.html)
- [BR BR PATAPIM OBBY CHALLENGE](https://studyquests.pages.dev/br-br-patapim-obby-challenge.html)
- [CAR ESCAPE](https://quizverses.github.io/car-escape.html)
- [HORSEBACK SURVIVAL](https://quizverses.github.io/horseback-survival.html)
- [KING OF THE HILL](https://quizverses.github.io/king-of-the-hill.html)
- [CATEGORY ADVENTURE](https://studyquesthub.web.app/category-adventure.html)
- [WIRE CONNECT](https://quizverses.github.io/wire-connect.html)
- [BLUE MUSHROOM CAT RUN](https://quizverses-9d2f2.web.app/blue-mushroom-cat-run.html)
- [CUNNING GINGER](https://studyplayings.pages.dev/cunning-ginger.html)
- [DRUNK BUT NOT WASTED KNIGHT](https://studyquests.github.io/drunk-but-not-wasted-knight.html)
- [CATEGORY 3D1 371](https://studyplaying.github.io/category-3d1-371.html)
- [MAHJONG ZEN GARDEN](https://studyplayings.pages.dev/mahjong-zen-garden.html)
- [HOOP WORLD 3D](https://studyquests.pages.dev/hoop-world-3d.html)
- [UNSCREW WOOD PUZZLE](https://quizverses-9d2f2.web.app/unscrew-wood-puzzle.html)
- [FISH SORT](https://studyquests.pages.dev/fish-sort.html)
- [SPIN SHOT SIEGE](https://studyquests.github.io/spin-shot-siege.html)
- [BUILD A GO KART](https://quizverses.github.io/build-a-go-kart.html)
- [CATEGORY BIKE 2](https://studyplayings.web.app/category-bike-2.html)
- [PLANE CRASH RAGDOLL SIMULATOR](https://studyplaying.github.io/plane-crash-ragdoll-simulator.html)
- [JEWEL LEGEND QUEST](https://quizverses.github.io/jewel-legend-quest.html)
- [CATEGORY TURN BASED](https://quizverses.github.io/category-turn-based.html)
- [ANGRY CITY SMASHER](https://studyplayings.pages.dev/angry-city-smasher.html)
- [MONEY FACTORY EARN A BILLION](https://studyquests.github.io/money-factory-earn-a-billion.html)
- [STICKER JAM PEEL OFF MATCH](https://quizverses.github.io/sticker-jam-peel-off-match.html)
- [CRAFT DRILL](https://studyquesthub.web.app/craft-drill.html)
- [CITYMIX SOLITAIRE](https://studyplaying.github.io/citymix-solitaire.html)
- [HEXA ARROWS PUZZLE](https://studyquests.pages.dev/hexa-arrows-puzzle.html)
- [TAXI DRIVER SIMULATOR](https://studyquests.github.io/taxi-driver-simulator.html)
- [CUBE STORIES ESCAPE](https://studyplayings.pages.dev/cube-stories-escape.html)
- [BLOONS SURVIVALIO](https://studyplayings.pages.dev/bloons-survivalio.html)
- [CATEGORY SIMULATION 2](https://studyquesthub.web.app/category-simulation-2.html)
- [SCHOOL ESCAPE OBBIE RUN](https://quizverses-9d2f2.web.app/school-escape-obbie-run.html)
- [BRICK MATCH](https://studyplaying.github.io/brick-match.html)
- [POPS QUEST](https://studyquests.github.io/pops-quest.html)
- [HORROR HOTEL SCARY ROOM](https://learnquester.github.io/horror-hotel-scary-room.html)
- [REAL MOTORBIKE SIMULATOR RACE 3D](https://quizverses-9d2f2.web.app/real-motorbike-simulator-race-3d.html)
- [HELIX CRUSH](https://studyquests.github.io/helix-crush.html)
- [INDEX9](https://studyquesthub.web.app/index9.html)
- [UNBLOCK BALL SLIDE PUZZLE](https://studyplayings.pages.dev/unblock-ball-slide-puzzle.html)
- [CATEGORY BATTLE](https://studyquests.github.io/category-battle.html)
- [ARCHERS RANDOM](https://studyplaying.github.io/archers-random.html)
- [CATEGORY MINECRAFT](https://quizverses-9d2f2.web.app/category-minecraft.html)
- [CATEGORY PHYSICS371](https://studyplayings.pages.dev/category-physics371.html)
- [FORMULA RACING GAMES CAR GAME](https://quizverses.pages.dev/formula-racing-games-car-game.html)
- [GOODELUXE](https://studyquests.github.io/goodeluxe.html)
- [EMOJI FRENZY](https://studyquesthub.web.app/emoji-frenzy.html)
- [MERGE COMBO](https://studyquests.github.io/merge-combo.html)
- [ZOMBIE ARENA 2 FURY ROAD](https://quizverses.pages.dev/zombie-arena-2-fury-road.html)
- [PRACTICE ON ME](https://quizverses.github.io/practice-on-me.html)
- [CATEGORY CASUAL](https://thelearnquester.web.app/category-casual.html)
- [CATEGORY BYEPASSHUB](https://thelearnquester.web.app/category-byepasshub.html)
- [HOLIDAY HEX SORT](https://studyquests.github.io/holiday-hex-sort.html)
- [DIGITAL CIRCUS RUN](https://studyplayings.pages.dev/digital-circus-run.html)
- [CATEGORY TANK](https://studyplayings.pages.dev/category-tank.html)
- [MAGIC FINGER PUZZLE 3D](https://studyquests.github.io/magic-finger-puzzle-3d.html)
- [CATEGORY PUZZLE 4](https://quizverses.github.io/category-puzzle-4.html)
- [CATEGORY RACING DRIVING](https://studyplayings.pages.dev/category-racing-driving.html)
- [SUDOKU BRAIN BLOCKS](https://studyplaying.github.io/sudoku-brain-blocks.html)
- [MUSHROOM BLOCKS](https://studyplayings.pages.dev/mushroom-blocks.html)
- [DRAW TO FISH FIGHT](https://studyquesthub.web.app/draw-to-fish-fight.html)
- [IDLE RESTAURANT TYCOON](https://studyplaying.github.io/idle-restaurant-tycoon.html)
- [CATEGORY BIKE 2](https://quizverses.github.io/category-bike-2.html)
- [CATEGORY AGILITY 2](https://studyplayings.pages.dev/category-agility-2.html)
- [CATEGORY POOL 2](https://studyplayings.pages.dev/category-pool-2.html)
- [BITBALL](https://studyquesthub.web.app/bitball.html)
- [CATEGORY PLATFORM260](https://quizverses-9d2f2.web.app/category-platform260.html)
- [POTION SORT](https://quizverses.pages.dev/potion-sort.html)
- [CATEGORY MMO24](https://studyplayings.pages.dev/category-mmo24.html)
- [CATEGORY DESTROY256](https://quizverses.github.io/category-destroy256.html)
- [CATEGORY SPORTS](https://quizverses-9d2f2.web.app/category-sports.html)
- [PEOPLE PLAYGROUND 3D](https://quizverses.github.io/people-playground-3d.html)
