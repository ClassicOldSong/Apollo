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
- [HOTFOOT BASEBALL](https://theskillquest.pages.dev/hotfoot-baseball.html)
- [OBBY YARD SALE](https://quizverses.github.io/obby-yard-sale.html)
- [FRAGEN](https://quizverses.pages.dev/fragen.html)
- [WEAPONS AND RAGDOLLS](https://studyquests.github.io/weapons-and-ragdolls.html)
- [EATING SIMULATOR](https://studyquests.github.io/eating-simulator.html)
- [CATEGORY MMO24](https://studyquesthub.web.app/category-mmo24.html)
- [MONEY FACTORY EARN A BILLION](https://studyquests.github.io/money-factory-earn-a-billion.html)
- [POP THE BUBBLE](https://studyquests.github.io/pop-the-bubble.html)
- [SOLITAIRE KLONDIKE](https://studyquests.github.io/solitaire-klondike.html)
- [SNAKE 2048](https://learnquester.github.io/snake-2048.html)
- [BOXTERIA](https://studyplaying.github.io/boxteria.html)
- [OFFROAD ISLAND](https://studyquests.github.io/offroad-island.html)
- [CATEGORY CONTROLLER](https://studyplayings.web.app/category-controller.html)
- [DUCK HUNTING OPEN SEASON](https://studyquests.github.io/duck-hunting-open-season.html)
- [MAD TRUCK](https://studyquests.github.io/mad-truck.html)
- [SPACE BLAST](https://studyquests.github.io/space-blast.html)
- [LAST PLAY RAGDOLL SANDBOX KQB](https://studyquests.github.io/last-play-ragdoll-sandbox-kqb.html)
- [MAX CRUSHER CRAZY DESTRUCTION AND CAR CRASHES](https://studyquests.github.io/max-crusher-crazy-destruction-and-car-crashes.html)
- [MEGA RAMP CAR STUNTS](https://studyquesthub.web.app/mega-ramp-car-stunts.html)
- [BLOCK DIGGER](https://studyquests.github.io/block-digger.html)
- [HYPERSPACE   QUANTUM FRACTURE FEZ](https://studyquests.github.io/hyperspace---quantum-fracture-fez.html)
- [SITEMAP](https://cryptotify.github.io/sitemap.html)
- [CATEGORY CARDS](https://thelearnquester.web.app/category-cards.html)
- [CATEGORY STORY45](https://studyplayings.pages.dev/category-story45.html)
- [SQUARE PUNKI LONG HAND](https://studyplayings.web.app/square-punki-long-hand.html)
- [MAHJONG FOUR RIVERS](https://studyquests.github.io/mahjong-four-rivers.html)
- [BOARD KINGS BOARD DICE](https://studyplayings.web.app/board-kings-board-dice.html)
- [CLASH RUN](https://learnquesters.pages.dev/clash-run.html)
- [SUMMER TRIPLE MAHJONG](https://studyquesthub.web.app/summer-triple-mahjong.html)
- [OBBY CARDS THE LEGEND HUNT](https://quizverses-9d2f2.web.app/obby-cards-the-legend-hunt.html)
- [CHROMA TREK](https://quizverses-9d2f2.web.app/chroma-trek.html)
- [STICKMAN SORT](https://learnquesters.pages.dev/stickman-sort.html)
- [TRACESOCCER UBC](https://studyquests.github.io/tracesoccer-ubc.html)
- [POWER LIGHT](https://learnquesters.pages.dev/power-light.html)
- [BACK 2 SCHOOL MAKEOVER](https://studyplaying.github.io/back-2-school-makeover.html)
- [STRATEGY OF WAR TANKS AND HELICOPTERS](https://learnquesters.pages.dev/strategy-of-war-tanks-and-helicopters.html)
- [FLAPPY RUSH](https://studyquests.pages.dev/flappy-rush.html)
- [FARM TILES HARVEST](https://quizverses-9d2f2.web.app/farm-tiles-harvest.html)
- [JUST DICE RANDOM TOWER DEFENCE](https://thelearnquester.web.app/just-dice-random-tower-defence.html)
- [SCREW PUZZLE MASTER](https://quizverses-9d2f2.web.app/screw-puzzle-master.html)
- [SORCERER MAHJONG MARVELS](https://studyquesthub.web.app/sorcerer-mahjong-marvels.html)
- [BRAIN FIND CAN YOU FIND IT](https://quizverses.pages.dev/brain-find-can-you-find-it.html)
- [ANGRY FLAPPY](https://studyquests.github.io/angry-flappy.html)
- [MINI OBBY WAR GAME](https://quizverses-9d2f2.web.app/mini-obby-war-game.html)
- [GLOVES GROW RUSH](https://studyquests.github.io/gloves-grow-rush.html)
- [BLOCK PARKOUR TRIALS](https://thelearnquester.web.app/block-parkour-trials.html)
- [PUZZLE ABOUT ORANGE](https://learnquester.github.io/puzzle-about-orange.html)
- [ROBOTS GONE WILD](https://quizverses-9d2f2.web.app/robots-gone-wild.html)
- [3D ACRYLIC NAIL NAIL ART GAME](https://quizverses-9d2f2.web.app/3d-acrylic-nail-nail-art-game.html)
- [CATEGORY IO](https://quizverses.github.io/category-io.html)
- [GEOMETRY VIBES MONSTER](https://studyquesthub.web.app/geometry-vibes-monster.html)
- [MOTO CABBIE SIMULATOR](https://quizverses.github.io/moto-cabbie-simulator.html)
- [TEACHER SIMULATOR CHRISTMAS EXAM](https://learnquesters.pages.dev/teacher-simulator-christmas-exam.html)
- [SHEEP SORT PUZZLE SORT COLOR](https://studyplaying.github.io/sheep-sort-puzzle-sort-color.html)
- [CATCH THIEF](https://studyquests.github.io/catch-thief.html)
- [MANSION STORY MATCH](https://studyquests.github.io/mansion-story-match.html)
- [MARBLE SORT](https://studyplaying.github.io/marble-sort.html)
- [WORLD WAR 2 SHOOTER](https://studyplaying.github.io/world-war-2-shooter.html)
- [FUTURE WAR BOT BATTLE IN SPACE 3D](https://studyquests.github.io/future-war-bot-battle-in-space-3d.html)
- [DATA DIGGERS](https://studyquests.github.io/data-diggers.html)
- [CATEGORY SOCCER 2](https://quizverses.pages.dev/category-soccer-2.html)
- [RUSSIAN TREASURE HUNTER](https://thelearnquester.web.app/russian-treasure-hunter.html)
- [MR BOUNCE](https://studyplayings.pages.dev/mr-bounce.html)
- [CATEGORY MOUSE1 707](https://quizverses.pages.dev/category-mouse1-707.html)
- [ONLINE PORTAL](https://quizverses.github.io/)
- [M5 CITY DRIVER](https://studyplayings.web.app/m5-city-driver.html)
- [MATCH MASTER](https://studyplaying.github.io/match-master.html)
- [SNOW RACE 3D FUN RACING](https://thelearnquester.web.app/snow-race-3d-fun-racing.html)
- [PAWS PALS DINER](https://studyquests.github.io/paws-pals-diner.html)
- [FASHION FAMOUS](https://studyquests.github.io/fashion-famous.html)
- [EGG FARM](https://thelearnquester.web.app/egg-farm.html)
- [ROBBIE BECOME A BEAST](https://studyquesthub.web.app/robbie-become-a-beast.html)
- [KINGS AND QUEENS MAHJONG](https://studyquests.github.io/kings-and-queens-mahjong.html)
- [MATCH FIND 3D](https://studyquests.github.io/match-find-3d.html)
- [CAPYBARA SUIKA](https://studyquests.pages.dev/capybara-suika.html)
- [CLAP CLAP NIGHTMARE](https://thelearnquester.web.app/clap-clap-nightmare.html)
- [OBBY PRISON RUN](https://studyquests.pages.dev/obby-prison-run.html)
- [ZOMBIE DEFENSE WAR](https://studyquests.github.io/zombie-defense-war.html)
- [WORDS MATCH](https://learnquester.github.io/words-match.html)
- [HEX SENSE](https://studyquests.github.io/hex-sense.html)
- [NEON BLAST](https://studyplaying.github.io/neon-blast.html)
- [CATEGORY MINIGAMES29](https://quizverses.github.io/category-minigames29.html)
- [ZOMBIE ARENA 2 FURY ROAD](https://studyplaying.github.io/zombie-arena-2-fury-road.html)
- [SLIME RUSH](https://thelearnquester.web.app/slime-rush.html)
- [CATEGORY EDUCATIONAL25](https://studyplayings.pages.dev/category-educational25.html)
- [KITTEN NEVER DIES](https://studyplayings.web.app/kitten-never-dies.html)
- [COUNTRYSIDE TRUCK DRIVE](https://quizverses-9d2f2.web.app/countryside-truck-drive.html)
- [ONLINE PORTAL](https://brainquests.github.io/)
- [BRAWL BROS SQUAD](https://thelearnquester.web.app/brawl-bros-squad.html)
- [ART SALON](https://quizverses.pages.dev/art-salon.html)
- [POPPY PLAYER PUZZLE](https://thelearnquester.web.app/poppy-player-puzzle.html)
- [DOLLYS RESTAURANT ORGANIZING](https://studyquests.pages.dev/dollys-restaurant-organizing.html)
- [BURGER CAFE COOKING GAMES FOR KIDS](https://learnquesters.pages.dev/burger-cafe-cooking-games-for-kids.html)
- [MOUNTAIN BUS DRIVER](https://learnquester.pages.dev/mountain-bus-driver.html)
- [CHRISTMAS SORTING](https://studyquests.github.io/christmas-sorting.html)
- [IBIZA FOAM PARTY](https://quizverses.pages.dev/ibiza-foam-party.html)
- [PRINCESSES OF QUADROBICS](https://thelearnquester.web.app/princesses-of-quadrobics.html)
- [MERGE SHOOTER](https://studyquests.github.io/merge-shooter.html)
- [MUSHROOM FEVER MATCH 3](https://quizverses-9d2f2.web.app/mushroom-fever-match-3.html)
- [ART SALON](https://studyplaying.github.io/art-salon.html)
- [INK INC TATTOO](https://learnquester.pages.dev/ink-inc-tattoo.html)
- [DELICIOUS EMILYS NEW BEGINNING VALENTINES EDITION](https://thelearnquester.web.app/delicious-emilys-new-beginning-valentines-edition.html)
- [FIND GOODS](https://quizverses.github.io/find-goods.html)
- [CATEGORY BOOKMARK](https://studyplaying.github.io/category-bookmark.html)
- [SHIPS 3D IO](https://learnquester.pages.dev/ships-3d-io.html)
- [SIGMA BOY MUSICAL CLICKER](https://thelearnquester.web.app/sigma-boy-musical-clicker.html)
- [WORD HUNT](https://thelearnquester.web.app/word-hunt.html)
- [PRINCESS RESCUE SAVE GIRL](https://studyplayings.web.app/princess-rescue-save-girl.html)
- [MY FARM EMPIRE](https://studyplaying.github.io/my-farm-empire.html)
- [DIY PHONE CASE SHOP](https://studyplaying.github.io/diy-phone-case-shop.html)
- [THE BIG HIT RUN](https://thelearnquester.web.app/the-big-hit-run.html)
- [MAHJONG SLIDE PUZZLE](https://learnquester.pages.dev/mahjong-slide-puzzle.html)
- [CATEGORY PUZZLE 4](https://studyplaying.github.io/category-puzzle-4.html)
- [BLOCKS BREAKER](https://studyplayings.web.app/blocks-breaker.html)
- [CATEGORY PHYSICS371](https://studyquests.pages.dev/category-physics371.html)
- [SPRUNKI SHOOTER 2025](https://learnquester.pages.dev/sprunki-shooter-2025.html)
- [DEFENDERS OF THE REALM AN EPIC WAR](https://studyplaying.github.io/defenders-of-the-realm-an-epic-war.html)
- [WOODEN BOLTS AND NUTS](https://quizverses-9d2f2.web.app/wooden-bolts-and-nuts.html)
- [INDIAN SUV OFFROAD SIMULATOR](https://studyquests.pages.dev/indian-suv-offroad-simulator.html)
- [GUESS THE ITALIAN BRAINROT ANIMALS](https://studyquests.github.io/guess-the-italian-brainrot-animals.html)
- [UNBLOCK IT 3D](https://studyplaying.github.io/unblock-it-3d.html)
- [BLOCK BUILDER JAM](https://quizverses.github.io/block-builder-jam.html)
- [VORTEX BALL](https://thelearnquester.web.app/vortex-ball.html)
- [PRINXY WINTERELLA](https://quizverses.pages.dev/prinxy-winterella.html)
- [EGG ADVENTURE](https://learnquester.pages.dev/egg-adventure.html)
- [GOBATTLEIO](https://studyplaying.github.io/gobattleio.html)
- [PULL THE THREAD PUZZLE](https://studyplaying.github.io/pull-the-thread-puzzle.html)
- [IDLE PIZZA BUSINESS](https://quizverses.github.io/idle-pizza-business.html)
- [MY ARCADE CENTER](https://studyplayings.web.app/my-arcade-center.html)
- [TRICKY ARROW 2](https://studyplayings.pages.dev/tricky-arrow-2.html)
