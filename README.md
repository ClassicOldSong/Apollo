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
- [MAHJONG CONNECT SPOOKY](https://themindplays.pages.dev/mahjong-connect-spooky.html)
- [CATEGORY BOOKMARK](https://thelearnquester.web.app/category-bookmark.html)
- [CUNNING GINGER](https://learnquesters.pages.dev/cunning-ginger.html)
- [SURVEV](https://quizverses-9d2f2.web.app/survev.html)
- [CATEGORY FREE RAGDOLL GAMES](https://learnquester.github.io/category-free-ragdoll-games.html)
- [BATTLE RACING STARS](https://quizverses.pages.dev/battle-racing-stars.html)
- [CATEGORY 2D1 070](https://studyquests.github.io/category-2d1-070.html)
- [Z STICK DUEL FIGHTING](https://studyplayings.pages.dev/z-stick-duel-fighting.html)
- [PICTURES RIDDLE](https://studyplayings.pages.dev/pictures-riddle.html)
- [MINI GOLF SAGA](https://studyplaying.github.io/mini-golf-saga.html)
- [ZOMBIE MONSTER SURVIVORS](https://studyplaying.github.io/zombie-monster-survivors.html)
- [BLOCK PUZZLE TROPICAL STORY](https://learnquester.github.io/block-puzzle-tropical-story.html)
- [CLUB TYCOON IDLE CLICKER](https://studyplaying.github.io/club-tycoon-idle-clicker.html)
- [MONSTER SLAYERS](https://studyplaying.github.io/monster-slayers.html)
- [HUNGRY CORGI CUTE MUSIC GAME](https://quizverses.github.io/hungry-corgi-cute-music-game.html)
- [CATEGORY UNBLOCKED GAMES](https://studyquests.github.io/category-unblocked-games.html)
- [UNTWIST ROAD](https://quizverses.github.io/untwist-road.html)
- [PUZZLE BLOCKS CLASSIC](https://studyquests.pages.dev/puzzle-blocks-classic.html)
- [BULL RUNNER](https://studyquests.github.io/bull-runner.html)
- [INDEX19](https://quizverses.pages.dev/index19.html)
- [POPPING SUSHI](https://learnquesters.pages.dev/popping-sushi.html)
- [CRAZY PLANE LANDING](https://learnquester.github.io/crazy-plane-landing.html)
- [CONNECT THE DOTS COLOR LINES](https://learnquester.github.io/connect-the-dots-color-lines.html)
- [ELEMENTAL DRESSUP MAGIC](https://studyplayings.pages.dev/elemental-dressup-magic.html)
- [CATEGORY SCHOOL](https://studyplaying.github.io/category-school.html)
- [PAPERWARIO](https://studyquests.github.io/paperwario.html)
- [CITY DRIFT RACING](https://studyplayings.pages.dev/city-drift-racing.html)
- [BRUTALMANIA IO](https://studyplaying.github.io/brutalmania-io.html)
- [PET SALON SIMULATOR](https://studyplayings.pages.dev/pet-salon-simulator.html)
- [SNEAKER ART](https://studyplaying.github.io/sneaker-art.html)
- [CATEGORY MAHJONG GAMES](https://thelearnquester.web.app/category-mahjong-games.html)
- [SORTING BALLS](https://studyplayings.pages.dev/sorting-balls.html)
- [MOTO TRAFFIC RIDER](https://studyplayings.pages.dev/moto-traffic-rider.html)
- [COUNT MASTER MATCH COLOR RUN](https://studyquesthub.web.app/count-master-match-color-run.html)
- [CATEGORY LIGHTSPEED FILTER](https://thelearnquester.web.app/category-lightspeed-filter.html)
- [CATEGORY 2D1 070](https://thelearnquesters.pages.dev/category-2d1-070.html)
- [HORSE RACING DERBY QUEST](https://learnquesters.pages.dev/horse-racing-derby-quest.html)
- [GT CHAMPIONSHIP ARCADE](https://studyquests.github.io/gt-championship-arcade.html)
- [CLOAK MASTER SHOOTER RUN](https://learnquesters.pages.dev/cloak-master-shooter-run.html)
- [MAX MIXED COCKTAILS](https://studyquests.pages.dev/max-mixed-cocktails.html)
- [MIRROR SHAPE](https://studyplayings.web.app/mirror-shape.html)
- [SLINGER BLOCK](https://studyplayings.pages.dev/slinger-block.html)
- [WORD GAME 2026](https://learnquesters.pages.dev/word-game-2026.html)
- [CATEGORY FLASH](https://quizverses-9d2f2.web.app/category-flash.html)
- [MR DISC SLINGSHOT STRIKE](https://thelearnquester.web.app/mr-disc-slingshot-strike.html)
- [OFFICE BRAWL ROOM SMASH](https://studyquests.github.io/office-brawl-room-smash.html)
- [NOOB FUN FISHING](https://learnquester.github.io/noob-fun-fishing.html)
- [FURRY KUNG FU](https://studyquesthub.web.app/furry-kung-fu.html)
- [CATEGORY BIKE](https://studyquesthub.web.app/category-bike.html)
- [DRAW BRIDGE BRAIN GAME](https://studyplayings.pages.dev/draw-bridge-brain-game.html)
- [CATEGORY ART](https://quizverses.pages.dev/category-art.html)
- [WOODY TAP BLOCK](https://quizverses.pages.dev/woody-tap-block.html)
- [CATEGORY MERGE224](https://thelearnquester.web.app/category-merge224.html)
- [MEATRIDER](https://learnquesters.pages.dev/meatrider.html)
- [CATEGORY MISSION207](https://studyquesthub.web.app/category-mission207.html)
- [INDEX7](https://quizverses.pages.dev/index7.html)
- [GRANNY RETURNS 3D EVIL DESTINY](https://learnquesters.pages.dev/granny-returns-3d-evil-destiny.html)
- [JUST DICE RANDOM TOWER DEFENCE](https://learnquesters.pages.dev/just-dice-random-tower-defence.html)
- [FALLLING JEWELS](https://quizverses.pages.dev/fallling-jewels.html)
- [CATEGORY SHOOTER](https://thelearnquester.web.app/category-shooter.html)
- [SCARY TEACHER 3D RETURNS](https://studyquests.github.io/scary-teacher-3d-returns.html)
- [CATEGORY CAN T STOP PLAYING215](https://thelearnquesters.pages.dev/category-can-t-stop-playing215.html)
- [WORDS WITH OWL](https://learnquester.github.io/words-with-owl.html)
- [CUBICA](https://studyquesthub.web.app/cubica.html)
- [DAN THE MAN](https://studyplaying.github.io/dan-the-man.html)
- [FLAG MASTER WORLD FLAGS QUIZ](https://studyplaying.github.io/flag-master-world-flags-quiz.html)
- [OBBY CHAMPIONS](https://quizverses.github.io/obby-champions.html)
- [MY PERFECT MINE](https://studyquests.pages.dev/my-perfect-mine.html)
- [DUCK HUNTING OPEN SEASON](https://studyquests.github.io/duck-hunting-open-season.html)
- [CATEGORY CASUAL 4](https://studyplaying.github.io/category-casual-4.html)
- [FUSION 2048](https://studyquests.github.io/fusion-2048.html)
- [TAIL GUN CHARLIE](https://quizverses.github.io/tail-gun-charlie.html)
- [FARM MATCH SEASONS 2](https://quizverses.github.io/farm-match-seasons-2.html)
- [CATEGORY MOUSE1 697](https://thelearnquester.web.app/category-mouse1-697.html)
- [WEST FRONTIER SHARPSHOOTER 3D](https://studyquests.github.io/west-frontier-sharpshooter-3d.html)
- [CATEGORY MINECRAFT 2](https://learnquester.pages.dev/category-minecraft-2.html)
- [TOP HOG](https://studyquests.github.io/top-hog.html)
- [JAILBREAK ESCAPE FROM PRISON](https://thelearnquester.web.app/jailbreak-escape-from-prison.html)
- [SORT WATER NOW](https://learnquesters.pages.dev/sort-water-now.html)
- [MASTER ADDICTION SOLITAIRE](https://learnquesters.pages.dev/master-addiction-solitaire.html)
- [TARCAT](https://studyplayings.pages.dev/tarcat.html)
- [GODS MIXER](https://studyquesthub.web.app/gods-mixer.html)
- [GOO SLIME JUMP](https://quizverses.github.io/goo-slime-jump.html)
- [CHRISTMAS CANDY ESCAPE 3D](https://studyquests.github.io/christmas-candy-escape-3d.html)
- [KINGDOM PUZZLES](https://learnquester.github.io/kingdom-puzzles.html)
- [MR LONG HAND](https://quizverses.github.io/mr-long-hand.html)
- [STELLAR MINES SPACE MINER](https://studyquests.github.io/stellar-mines-space-miner.html)
- [CATEGORY MMO25](https://thelearnquester.web.app/category-mmo25.html)
- [STOP THE BULLET](https://studyplayings.pages.dev/stop-the-bullet.html)
- [BULLET HEROES](https://studyplaying.github.io/bullet-heroes.html)
- [CATEGORY MATCH 3](https://thelearnquester.web.app/category-match-3.html)
- [CATEGORY COOKING46](https://quizverses.pages.dev/category-cooking46.html)
- [PUZZLE PLAY](https://studyplayings.pages.dev/puzzle-play.html)
- [BROKEN CITY COMBAT](https://studyplayings.pages.dev/broken-city-combat.html)
- [AMONG SQUID CHALLENGE ONLINE](https://studyquests.pages.dev/among-squid-challenge-online.html)
- [MAGIC BEAUTY MAKEUP](https://quizverses.github.io/magic-beauty-makeup.html)
- [SPACE PIN MASTER PULL PIN PUZZLE](https://quizverses.github.io/space-pin-master-pull-pin-puzzle.html)
- [GEOMETRY VIBES MONSTER](https://quizverses.github.io/geometry-vibes-monster.html)
- [REAL DRIVING SIMULATOR](https://studyplaying.github.io/real-driving-simulator.html)
- [CATEGORY SOLITAIRE27](https://studyquests.github.io/category-solitaire27.html)
- [FNF 2 PLAYER](https://studyplaying.github.io/fnf-2-player.html)
- [IDLE LANDMARK BUILDER](https://studyplaying.github.io/idle-landmark-builder.html)
- [SPRUNKI CHARACTER MAKER OC](https://studyplayings.pages.dev/sprunki-character-maker-oc.html)
- [CATEGORY GAMES](https://learnquester.pages.dev/category-games.html)
- [KUZBASS HORROR](https://studyplaying.github.io/kuzbass-horror.html)
- [TOWER OF HELL OBBY BLOX](https://quizverses.pages.dev/tower-of-hell-obby-blox.html)
- [EXCAVATOR SIMULATOR 3D](https://learnquester.github.io/excavator-simulator-3d.html)
- [MINE SWEEPER](https://studyplayings.pages.dev/mine-sweeper.html)
- [EXIT PUZZLE](https://quizverses.github.io/exit-puzzle.html)
- [DUMMIES WORLD CUP](https://studyplayings.pages.dev/dummies-world-cup.html)
- [DUDU ENGINEERING TRUCK](https://studyplayings.pages.dev/dudu-engineering-truck.html)
- [QUEENS ROYAL SUDOKU PUZZLE](https://quizverses.pages.dev/queens-royal-sudoku-puzzle.html)
- [VOLLEY BEAN](https://studyquests.github.io/volley-bean.html)
- [BUBBLE SKY](https://studyquests.pages.dev/bubble-sky.html)
- [MINI SHOOTERS](https://quizverses-9d2f2.web.app/mini-shooters.html)
- [INDEX3](https://studyplaying.github.io/index3.html)
- [AIR STRIKE 2D](https://quizverses.github.io/air-strike-2d.html)
- [ROTATE RINGS CIRCLE PUZZLE](https://studyplaying.github.io/rotate-rings-circle-puzzle.html)
- [SPRUNKI 3D SHOOTER](https://studyplayings.pages.dev/sprunki-3d-shooter.html)
- [TRIPEAKS SOLITAIRE ESCAPES](https://learnquester.github.io/tripeaks-solitaire-escapes.html)
- [LADY POOL](https://quizverses.github.io/lady-pool.html)
- [CATEGORY STICKMAN](https://studyquesthub.web.app/category-stickman.html)
- [WAVE ROAD 3D](https://studyquesthub.web.app/wave-road-3d.html)
- [FREE THE BALL](https://learnquesters.pages.dev/free-the-ball.html)
- [I8 CITY DRIVER](https://quizverses.github.io/i8-city-driver.html)
- [3D ACRYLIC NAIL NAIL ART GAME](https://quizverses-9d2f2.web.app/3d-acrylic-nail-nail-art-game.html)
- [STUPIDITY TEST](https://studyplayings.pages.dev/stupidity-test.html)
- [BACKROOMS](https://studyplaying.github.io/backrooms.html)
- [TIED UP](https://studyplayings.web.app/tied-up.html)
- [CAR CRASH TEST ABANDONED CITY](https://learnquester.github.io/car-crash-test-abandoned-city.html)
