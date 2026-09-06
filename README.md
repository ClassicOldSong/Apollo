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
- [MAHJONG CONNECT COOKWARE](https://studyquests.github.io/mahjong-connect-cookware.html)
- [UNBLOCK IT ATLANTIS](https://themindzone.pages.dev/unblock-it-atlantis.html)
- [CLUB TYCOON IDLE CLICKER](https://studyplaying.github.io/club-tycoon-idle-clicker.html)
- [GOLF MINI](https://themindskillplayplay.pages.dev/golf-mini.html)
- [CATEGORY POOL17](https://themindzone.pages.dev/category-pool17.html)
- [CATEGORY SHOOTER 3](https://iskillplay.web.app/category-shooter-3.html)
- [CS COMMAND SNIPERS](https://studyquests.github.io/cs-command-snipers.html)
- [TIP TAP](https://iskillplay.web.app/tip-tap.html)
- [BUBLIX BUBBLE HIT](https://studyplayings.web.app/bublix-bubble-hit.html)
- [CATEGORY PUZZLE 3](https://studyquests.github.io/category-puzzle-3.html)
- [ASSASSIN COMMANDO CAR DRIVING](https://theskillquest.pages.dev/assassin-commando-car-driving.html)
- [JAVELIN BATTLE](https://quizverses.pages.dev/javelin-battle.html)
- [BLUE GIRLS MAKEUP](https://quizverses.pages.dev/blue-girls-makeup.html)
- [INDEX31](https://thelearnquesters.pages.dev/index31.html)
- [CATEGORY CASUAL](https://themindzone.pages.dev/category-casual.html)
- [SHANGHAI CHEF](https://skillplay.github.io/shanghai-chef.html)
- [DONNES SPINNING WORLD](https://theskillquest.pages.dev/donnes-spinning-world.html)
- [PHOTO BLOCK JOURNEY](https://skillplay.github.io/photo-block-journey.html)
- [CANDY MATCH 4](https://iskillplay.web.app/candy-match-4.html)
- [OBBY HIGHEST JUMP EVER](https://thelearnquesters.pages.dev/obby-highest-jump-ever.html)
- [POP THEM](https://skillplay.github.io/pop-them.html)
- [DRAW BRIDGE PUZZLE](https://studyplaying.github.io/draw-bridge-puzzle.html)
- [BUNNY BOY ONLINE](https://themindzone.pages.dev/bunny-boy-online.html)
- [CAR OUT JAM](https://iskillquest.pages.dev/car-out-jam.html)
- [WORD SEARCH UNIVERSE](https://skillplay.github.io/word-search-universe.html)
- [PUZZLE PLAY](https://quizverses.github.io/puzzle-play.html)
- [CATEGORY INTERSTELLARPROXY](https://studyquests.github.io/category-interstellarproxy.html)
- [CUT N FILL](https://iskillplay.web.app/cut-n-fill.html)
- [BUBBLE SHOOTER NEON](https://studyquests.github.io/bubble-shooter-neon.html)
- [67 CLICKER](https://studyquests.github.io/67-clicker.html)
- [MAHJONG TRIPLE 3D TILE MATCH](https://studyplaying.github.io/mahjong-triple-3d-tile-match.html)
- [HOTFOOT BASEBALL](https://studyquests.github.io/hotfoot-baseball.html)
- [ANGRY FLAPPY](https://studyquests.github.io/angry-flappy.html)
- [ANIMAL BASKETBALL](https://theskillquest.pages.dev/animal-basketball.html)
- [JUMP IN TO THE PLANE](https://iskillquest.pages.dev/jump-in-to-the-plane.html)
- [TOILET TIME](https://thelearnquester.web.app/toilet-time.html)
- [CITYQUEST](https://themindzone.pages.dev/cityquest.html)
- [STRATEGY OF WAR TANKS AND HELICOPTERS](https://learnquesters.pages.dev/strategy-of-war-tanks-and-helicopters.html)
- [SQUARE PUNKI LONG HAND](https://quizverses.pages.dev/square-punki-long-hand.html)
- [ARMY DEFENCE DINO SHOOT](https://iskillquest.pages.dev/army-defence-dino-shoot.html)
- [OVERPROTECTIVE BOYFRIEND](https://skillplay.github.io/overprotective-boyfriend.html)
- [AFRICAN PRINCESSES STYLE ISLAND](https://learnquester.pages.dev/african-princesses-style-island.html)
- [ITALIAN ANIMALS CREATE YOUR OWN BRAINROT](https://iskillquest.pages.dev/italian-animals-create-your-own-brainrot.html)
- [SNOW ROAD PUZZLE](https://studyquests.github.io/snow-road-puzzle.html)
- [ELLIES RECIPE DUBAI CHOCOLATE BAR](https://iskillquest.pages.dev/ellies-recipe-dubai-chocolate-bar.html)
- [ITALIAN BRAINROT SURVIVE PARKOUR](https://iskillquest.pages.dev/italian-brainrot-survive-parkour.html)
- [ONE LINE DRAWING](https://themindzone.pages.dev/one-line-drawing.html)
- [CATEGORY FREE](https://studyquests.github.io/category-free.html)
- [CRYPTO GALS TIKTOK FASHION](https://studyplayings.pages.dev/crypto-gals-tiktok-fashion.html)
- [CATEGORY DEFENSE](https://quizverses.github.io/category-defense.html)
- [BACKFLIP MASTER](https://iskillplay.web.app/backflip-master.html)
- [POOL MASTER](https://thequizzone.pages.dev/pool-master.html)
- [TYPING ADVENTURE](https://quizverses.github.io/typing-adventure.html)
- [CATEGORY ARMY40](https://iskillquest.pages.dev/category-army40.html)
- [BLOCK SNIPER](https://studyquests.github.io/block-sniper.html)
- [TRI PEAKS EMERLAND SOLITAIRE](https://studyquests.github.io/tri-peaks-emerland-solitaire.html)
- [SAVE THE CROP](https://iskillplay.web.app/save-the-crop.html)
- [COLOR MAZE](https://learnquesters.pages.dev/color-maze.html)
- [CATEGORY CASUAL 12](https://quizverses.github.io/category-casual-12.html)
- [SCHOOLBOY RUNAWAY ROOM ESCAPE](https://skillplay.github.io/schoolboy-runaway-room-escape.html)
- [ROLLING BALLS SEA RACE](https://learnquester.github.io/rolling-balls-sea-race.html)
- [PING PONG BATTLE TABLE TENNIS](https://skillplay.github.io/ping-pong-battle-table-tennis.html)
- [MERGE SQUARES](https://studyquests.github.io/merge-squares.html)
- [DINO GAME](https://theskillquest.pages.dev/dino-game.html)
- [GET TO THE CHOPPER](https://quizverses-9d2f2.web.app/get-to-the-chopper.html)
- [CAKE MERGE 2](https://learnquesters.pages.dev/cake-merge-2.html)
- [CATEGORY CAR376](https://quizverses-9d2f2.web.app/category-car376.html)
- [CATEGORY QUIZ](https://learnquester.pages.dev/category-quiz.html)
- [STREET TRAFFIC RACER](https://studyquests.github.io/street-traffic-racer.html)
- [DETECTIVE LOGIC PUZZLES](https://iskillquest.pages.dev/detective-logic-puzzles.html)
- [ONE LINE](https://quizverses.pages.dev/one-line.html)
- [SKILLFITE IO](https://iskillplay.web.app/skillfite-io.html)
- [HOTGEAR](https://themindzone.pages.dev/hotgear.html)
- [CONTAINER SORT PUZZLE](https://learnquester.github.io/container-sort-puzzle.html)
- [GRAND MAHJONG](https://iskillplay.web.app/grand-mahjong.html)
- [SUMMER SPOTLIGHT DIFFERENCES](https://studyplayings.web.app/summer-spotlight-differences.html)
- [PUZZLE BLOCKS FILL IT COMPLETELY](https://studyplayings.web.app/puzzle-blocks-fill-it-completely.html)
- [PIXEL PATH](https://quizverses.github.io/pixel-path.html)
- [IDOL LIVESTREAM DOLL DRESS UP](https://iskillplay.web.app/idol-livestream-doll-dress-up.html)
- [SPRUNKI MATCH](https://quizverses.pages.dev/sprunki-match.html)
- [FALLING ART RAGDOLL SIMULATOR](https://skillplay.github.io/falling-art-ragdoll-simulator.html)
- [BOBBLEHEAD BALL](https://iskillquest.pages.dev/bobblehead-ball.html)
- [SHAPE TRANSFORM RACE](https://studyplayings.pages.dev/shape-transform-race.html)
- [YUMMY TALES 4](https://quizverses.pages.dev/yummy-tales-4.html)
- [MAGIC BEAUTY MAKEUP](https://skillplay.github.io/magic-beauty-makeup.html)
- [WAVE ROAD 3D](https://studyquests.github.io/wave-road-3d.html)
- [CATEGORY PREMIUM PERKS74](https://iskillplay.web.app/category-premium-perks74.html)
- [FRUITSLAND ESCAPE FROM THE AMUSEMENT PARK](https://theskillquest.pages.dev/fruitsland-escape-from-the-amusement-park.html)
- [ROCKET SKY](https://skillplay.github.io/rocket-sky.html)
- [GOAL IO](https://themindplay.github.io/goal-io.html)
- [BANG BANG MAHJONG](https://studyquests.github.io/bang-bang-mahjong.html)
- [INDEX18](https://learnquester.pages.dev/index18.html)
- [FIGHT TO THE END](https://quizverses.pages.dev/fight-to-the-end.html)
- [CANDY RAIN 5](https://skillplay.github.io/candy-rain-5.html)
- [SPIDER ROPE HERO CITY FIGHT](https://skillplay.github.io/spider-rope-hero-city-fight.html)
- [LABUBU JETPACK RUSH](https://skillplay.github.io/labubu-jetpack-rush.html)
- [GOON BALL](https://themindplay.github.io/goon-ball.html)
- [REFLECT BEAM LASER LOGIC](https://themindplay.github.io/reflect-beam-laser-logic.html)
- [CATEGORY PUZZLE 6](https://iskillplay.web.app/category-puzzle-6.html)
- [DOT BY DOT](https://theskillquest.pages.dev/dot-by-dot.html)
- [BARBEE MET GALA TRANSFORMATION](https://learnquesters.pages.dev/barbee-met-gala-transformation.html)
- [DIRTY MONEY THE RICH GET RICH](https://iskillplay.web.app/dirty-money-the-rich-get-rich.html)
- [CATEGORY STICKMAN 3](https://iskillplay.web.app/category-stickman-3.html)
- [ZOMBIE RODEO MULTIPLICATION](https://themindskillplayplay.pages.dev/zombie-rodeo-multiplication.html)
- [POPPY PLAYER PUZZLE](https://learnquester.pages.dev/poppy-player-puzzle.html)
- [SMASH THE BOTTLE](https://themindzone.pages.dev/smash-the-bottle.html)
- [BOTTLE CHALLENGE](https://themindzone.pages.dev/bottle-challenge.html)
- [GRAND CLASH ARENA](https://thequizzone.pages.dev/grand-clash-arena.html)
- [ARROWS PUZZLE ESCAPE](https://themindzone.pages.dev/arrows-puzzle-escape.html)
- [MAGIC BRICK WARS](https://skillplay.github.io/magic-brick-wars.html)
- [FAR ORION NEW WORLDS](https://iskillplay.web.app/far-orion-new-worlds.html)
- [SUPER NINJA BALLOON](https://learnquester.github.io/super-ninja-balloon.html)
- [CHAMPIONS FC](https://studyquests.github.io/champions-fc.html)
- [CATEGORY CONTROLLER59](https://learnquester.pages.dev/category-controller59.html)
- [CRAZY BUS STATION](https://themindzone.pages.dev/crazy-bus-station.html)
- [BUBBLE SHOOTER REMASTERED](https://themindzone.pages.dev/bubble-shooter-remastered.html)
- [GEOMETRY VIBES X ARROW](https://studyplayings.web.app/geometry-vibes-x-arrow.html)
- [AVATAR MASTER FIX UP FACE](https://thequizzone.pages.dev/avatar-master-fix-up-face.html)
- [SLINGER BLOCK](https://learnquesters.pages.dev/slinger-block.html)
- [CATEGORY MMO25](https://learnquester.github.io/category-mmo25.html)
- [CAR FIGHTER](https://quizverses.pages.dev/car-fighter.html)
- [CHRISTMAS BLIND BOX](https://studyquests.github.io/christmas-blind-box.html)
- [MAGIC FINGER PUZZLE 3D](https://skillplay.github.io/magic-finger-puzzle-3d.html)
- [CATEGORY ESCAPE 2](https://quizverses.github.io/category-escape-2.html)
- [NUTS BOLTS SORT COLOR PUZZLE](https://learnquester.github.io/nuts-bolts-sort-color-puzzle.html)
- [PIXEL DESTROYER](https://studyquests.github.io/pixel-destroyer.html)
- [CAT EVOLUTION](https://quizverses.pages.dev/cat-evolution.html)
- [PRINXY HOUSE OF FASHION](https://thequizzone.pages.dev/prinxy-house-of-fashion.html)
- [BLOCK DIGGER](https://theskillquest.pages.dev/block-digger.html)
- [ROYAL JIGSAW](https://iskillplay.web.app/royal-jigsaw.html)
