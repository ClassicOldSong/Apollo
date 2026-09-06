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
- [ONLINE PORTAL](https://cryptotify.github.io/)
- [CATEGORY TURN BASED30](https://theskillquest.pages.dev/category-turn-based30.html)
- [CATEGORY BOOKMARKLETS](https://thelearnquester.web.app/category-bookmarklets.html)
- [CATEGORY MAHJONG CONNECT](https://quizverses.pages.dev/category-mahjong-connect.html)
- [ENCHANTED EASTER ADVENTURE](https://studyplayings.web.app/enchanted-easter-adventure.html)
- [PRINCESS RESCUE SAVE GIRL](https://learnquester.github.io/princess-rescue-save-girl.html)
- [MY CASTLE MERGE STORY](https://studyplayings.web.app/my-castle-merge-story.html)
- [HORROR MINECRAFT PARTYTIME](https://quizverses.pages.dev/horror-minecraft-partytime.html)
- [CATEGORY BUBBLE SHOOTER](https://quizverses.pages.dev/category-bubble-shooter.html)
- [MOLANG MATCHN MUNCH](https://studyplayings.pages.dev/molang-matchn-munch.html)
- [CHRISTMAS FIND THE DIFFERENCES](https://studyplayings.web.app/christmas-find-the-differences.html)
- [BLOCK CRAFT 3D](https://studyplayings.pages.dev/block-craft-3d.html)
- [PRIVACY](https://studyquesthub.web.app/privacy.html)
- [CATEGORY TOWER DEFENSE](https://studyplayings.pages.dev/category-tower-defense.html)
- [CATEGORY MANAGEMENT209](https://studyplayings.pages.dev/category-management209.html)
- [BOXTERIA](https://studyplayings.web.app/boxteria.html)
- [LET THE TRAIN GO](https://learnquester.github.io/let-the-train-go.html)
- [IDLE BANK](https://studyplayings.web.app/idle-bank.html)
- [CATEGORY ROGUELIKE38](https://learnquester.github.io/category-roguelike38.html)
- [MINI GRAND THEFT CITY](https://studyplayings.web.app/mini-grand-theft-city.html)
- [INDEX5](https://quizverses.pages.dev/index5.html)
- [BLAST CUBES](https://quizverses.pages.dev/blast-cubes.html)
- [LAST PLAY RAGDOLL SANDBOX KQB](https://learnquester.github.io/last-play-ragdoll-sandbox-kqb.html)
- [BACK 2 SCHOOL MAKEOVER](https://quizverses.pages.dev/back-2-school-makeover.html)
- [SPRUNKI CHALLENGE](https://learnquester.github.io/sprunki-challenge.html)
- [ONLINE CAR DESTRUCTION SIMULATOR 3D](https://quizverses.github.io/online-car-destruction-simulator-3d.html)
- [PYRAMID SOLITAIRE ANCIENT EGYPT](https://studyplayings.web.app/pyramid-solitaire-ancient-egypt.html)
- [ITALIAN BRAINROT CHALLENGE](https://learnquester.github.io/italian-brainrot-challenge.html)
- [ARROWS PUZZLE ESCAPE](https://quizverses-9d2f2.web.app/arrows-puzzle-escape.html)
- [CATEGORY UNBLOCKERS](https://learnquester.github.io/category-unblockers.html)
- [CATEGORY TOWER DEFENSE 2](https://studyplayings.web.app/category-tower-defense-2.html)
- [OMG WORD RAINBOW](https://quizverses.pages.dev/omg-word-rainbow.html)
- [GLUTTONY](https://studyplayings.pages.dev/gluttony.html)
- [IDLE FACTORY DOMINATION](https://studyplayings.web.app/idle-factory-domination.html)
- [SCHOOLBOY RUNAWAY ROOM ESCAPE](https://learnquester.github.io/schoolboy-runaway-room-escape.html)
- [US ARMY CAR GAMES TRUCK DRIVING](https://learnquester.github.io/us-army-car-games-truck-driving.html)
- [CAT EVOLUTION 2](https://quizverses.github.io/cat-evolution-2.html)
- [EARWAX CLINIC](https://studyplayings.pages.dev/earwax-clinic.html)
- [FOOD TOWER DEFENSE](https://learnquester.github.io/food-tower-defense.html)
- [CUTE CRAFT LAB](https://quizverses.pages.dev/cute-craft-lab.html)
- [SAND BLAST](https://studyplayings.pages.dev/sand-blast.html)
- [CELEBRITY SPRING FASHION TRENDS](https://studyplayings.web.app/celebrity-spring-fashion-trends.html)
- [CATEGORY HORROR 3](https://quizverses.github.io/category-horror-3.html)
- [SCREW JAM FUN PUZZLE GAME](https://quizverses.pages.dev/screw-jam-fun-puzzle-game.html)
- [HAPPY FARM THE CROP](https://quizverses.github.io/happy-farm-the-crop.html)
- [POP THEM](https://quizverses.pages.dev/pop-them.html)
- [CATEGORY MERGE221](https://studyplayings.pages.dev/category-merge221.html)
- [ACE CAR RACING](https://quizverses.pages.dev/ace-car-racing.html)
- [CATEGORY SPORTS](https://learnquester.github.io/category-sports.html)
- [WATER SORT](https://quizverses.pages.dev/water-sort.html)
- [CATEGORY MAKEUP](https://quizverses.pages.dev/category-makeup.html)
- [CHICKEN WILD RUN](https://studyplayings.pages.dev/chicken-wild-run.html)
- [GEOMETRY ARROW 2](https://quizverses-9d2f2.web.app/geometry-arrow-2.html)
- [CATEGORY FARMING87](https://quizverses.github.io/category-farming87.html)
- [MAKE TWO](https://studyplayings.pages.dev/make-two.html)
- [KITTY MATCH 3 PUZZLE GAME](https://learnquester.github.io/kitty-match-3-puzzle-game.html)
- [DINOSAUR CARDS](https://studyplayings.web.app/dinosaur-cards.html)
- [CATEGORY MAHJONG 2](https://studyplayings.pages.dev/category-mahjong-2.html)
- [WORLD SOCCER](https://studyplayings.web.app/world-soccer.html)
- [FLOWER BLOCK](https://quizverses.pages.dev/flower-block.html)
- [SHAPE TRANSFORM RACE](https://studyplayings.web.app/shape-transform-race.html)
- [ARCHER DUNGEON HERO](https://quizverses-9d2f2.web.app/archer-dungeon-hero.html)
- [CATEGORY CUTE62](https://quizverses.github.io/category-cute62.html)
- [THE WHITE ROOM 5](https://learnquester.github.io/the-white-room-5.html)
- [2048 MAYHEMIO](https://learnquester.github.io/2048-mayhemio.html)
- [CATEGORY MERGE221](https://thelearnquester.web.app/category-merge221.html)
- [AUTUMN GLAM GALA](https://quizverses.pages.dev/autumn-glam-gala.html)
- [GAS STATION STICK SIMULATOR](https://learnquester.github.io/gas-station-stick-simulator.html)
- [SUPER DOG HERO DASH](https://studyplayings.pages.dev/super-dog-hero-dash.html)
- [CATEGORY POINT AND CLICK123](https://quizverses.pages.dev/category-point-and-click123.html)
- [KINGDOM PUZZLES](https://quizverses-9d2f2.web.app/kingdom-puzzles.html)
- [HAPPY GLASS GAME](https://quizverses.pages.dev/happy-glass-game.html)
- [CATEGORY DRAGON22](https://quizverses.github.io/category-dragon22.html)
- [CATEGORY HERO72](https://studyquests.github.io/category-hero72.html)
- [CATEGORY BATTLE](https://quizverses.github.io/category-battle.html)
- [HEXANAUT IO](https://studyplayings.pages.dev/hexanaut-io.html)
- [HERO RAGDOLL FIGHTING](https://learnquester.github.io/hero-ragdoll-fighting.html)
- [CATEGORY TOWER DEFENSE 2](https://studyquests.github.io/category-tower-defense-2.html)
- [CATEGORY SOLDIER](https://quizverses.pages.dev/category-soldier.html)
- [MATH BLOCK](https://studyplayings.web.app/math-block.html)
- [COLOR BUMP DANCER](https://quizverses.github.io/color-bump-dancer.html)
- [STACK UP](https://studyplayings.pages.dev/stack-up.html)
- [TOSS THE RING](https://studyplaying.github.io/toss-the-ring.html)
- [STRAWBERRY HERO](https://studyquests.pages.dev/strawberry-hero.html)
- [WORD SEARCH UNIVERSE ANIMALS](https://quizverses.github.io/word-search-universe-animals.html)
- [MAX SPEED](https://learnquester.github.io/max-speed.html)
- [DAY OF ATROCITY](https://learnquester.github.io/day-of-atrocity.html)
- [21 CARDS](https://quizverses-9d2f2.web.app/21-cards.html)
- [WORDS WITH PROF WISELY](https://studyquests.pages.dev/words-with-prof-wisely.html)
- [CATEGORY MATCH 3 2](https://studyquests.github.io/category-match-3-2.html)
- [RESTAURANT VIP MASTERCHEF](https://studyquests.pages.dev/restaurant-vip-masterchef.html)
- [INDEX20](https://studyplaying.github.io/index20.html)
- [CATEGORY DEFENSE](https://studyplaying.github.io/category-defense.html)
- [CLEAN THE FLOOR](https://studyquests.github.io/clean-the-floor.html)
- [DRIVER MASTER SIMULATOR](https://quizverses.pages.dev/driver-master-simulator.html)
- [CATEGORY CAR](https://learnquester.github.io/category-car.html)
- [2 PLAYER GAMES KIDS KITCHEN](https://studyplayings.pages.dev/2-player-games-kids-kitchen.html)
- [CATEGORY EDUCATIONAL](https://studyplaying.github.io/category-educational.html)
- [HEROES OF THE ARENA](https://quizverses.pages.dev/heroes-of-the-arena.html)
- [CRAFT MAN VS GIANT TNT](https://studyquests.pages.dev/craft-man-vs-giant-tnt.html)
- [BUBBLE UP](https://quizverses.pages.dev/bubble-up.html)
- [BARREL ROLLER AMAZING RUNNER](https://quizverses.pages.dev/barrel-roller-amazing-runner.html)
- [CATEGORY DRESS UP](https://thelearnquester.web.app/category-dress-up.html)
- [CATEGORY DRESS UP 2](https://studyquests.pages.dev/category-dress-up-2.html)
- [ZOMBIE CHASE](https://quizverses.pages.dev/zombie-chase.html)
- [CATEGORY MERGE](https://quizverses.pages.dev/category-merge.html)
- [CATEGORY FASHION105](https://thelearnquester.web.app/category-fashion105.html)
- [IDLE FIREFIGHTER 3D](https://studyquests.pages.dev/idle-firefighter-3d.html)
- [INDEX4](https://studyplaying.github.io/index4.html)
- [FASHION VALKYRIES SAGA OF STYLE](https://quizverses.pages.dev/fashion-valkyries-saga-of-style.html)
- [CATEGORY SIMULATION 4](https://studyquests.github.io/category-simulation-4.html)
- [WORDLING DAILY WORD CHALLENGE](https://learnquester.github.io/wordling-daily-word-challenge.html)
- [CATEGORY GOGUARDIAN](https://studyquests.github.io/category-goguardian.html)
- [MAHJONG TILE CLUB](https://quizverses.pages.dev/mahjong-tile-club.html)
- [BUBLIX BUBBLE HIT](https://studyplayings.web.app/bublix-bubble-hit.html)
- [SQUIDGAMEIO](https://learnquester.github.io/squidgameio.html)
- [CATEGORY MOBILE2 112](https://studyquests.github.io/category-mobile2-112.html)
- [SIGMA BOY MUSICAL CLICKER](https://studyplayings.web.app/sigma-boy-musical-clicker.html)
- [CATEGORY CASUAL 11](https://studyplaying.github.io/category-casual-11.html)
- [3 TILES](https://quizverses.pages.dev/3-tiles.html)
- [CATEGORY ADVENTURE 4](https://studyquests.github.io/category-adventure-4.html)
- [URUS CITY DRIVER](https://quizverses.pages.dev/urus-city-driver.html)
- [CATEGORY MOBILE2 112](https://studyquests.pages.dev/category-mobile2-112.html)
- [LEXY](https://quizverses.pages.dev/lexy.html)
- [GRILL IT ALL](https://studyquests.pages.dev/grill-it-all.html)
- [SEA BATTLE ADMIRAL](https://quizverses.pages.dev/sea-battle-admiral.html)
- [ANOMALY CONTENT RECORD](https://studyplayings.pages.dev/anomaly-content-record.html)
- [CATEGORY DRESS UP97](https://quizverses-9d2f2.web.app/category-dress-up97.html)
- [INDEX24](https://studyquests.github.io/index24.html)
- [MOTORCYCLE RACER ROAD MAYHEM](https://quizverses.pages.dev/motorcycle-racer-road-mayhem.html)
