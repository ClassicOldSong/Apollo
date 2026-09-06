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
- [LAST TO LEAVE CIRCLE OBBY](https://quizverses.pages.dev/last-to-leave-circle-obby.html)
- [FUNNY FEVER HOSPITAL](https://quizverses-9d2f2.web.app/funny-fever-hospital.html)
- [CATEGORY OBSTACLE](https://learnquester.pages.dev/category-obstacle.html)
- [UNICORN FIND THE DIFFERENCES](https://quizverses.github.io/unicorn-find-the-differences.html)
- [HEADLESS JOE](https://quizverses.github.io/headless-joe.html)
- [MONEY PING PONG](https://quizverses.github.io/money-ping-pong.html)
- [WORDLING DAILY WORD CHALLENGE](https://quizverses.github.io/wordling-daily-word-challenge.html)
- [INDEX24](https://quizverses.github.io/index24.html)
- [12 MINUTE ESCAPE](https://quizverses.github.io/12-minute-escape.html)
- [US ARMY CAR GAMES TRUCK DRIVING](https://quizverses.github.io/us-army-car-games-truck-driving.html)
- [UNTANGLE RINGS MASTER](https://studyplaying.github.io/untangle-rings-master.html)
- [TSUNAMI BRAINROTS ONLINE](https://studyplaying.github.io/tsunami-brainrots-online.html)
- [CATEGORY JIGSAW](https://quizverses.github.io/category-jigsaw.html)
- [BUBBLE SHOOTER VINTAGE](https://quizverses.github.io/bubble-shooter-vintage.html)
- [TERMS](https://cryptotify.netlify.app/terms.html)
- [CLICKER HERO](https://studyplaying.github.io/clicker-hero.html)
- [SORTING BALLS](https://studyplayings.pages.dev/sorting-balls.html)
- [CATEGORY POOL 2](https://quizverses.github.io/category-pool-2.html)
- [MY CAKE SHOP BAKE SERVE](https://quizverses.github.io/my-cake-shop-bake-serve.html)
- [STICKMAN MINERS WARS](https://learnquester.github.io/stickman-miners-wars.html)
- [OFF ROAD OVERDRIVE](https://studyplayings.pages.dev/off-road-overdrive.html)
- [ZOOMA DRAGON](https://quizverses.github.io/zooma-dragon.html)
- [TERMS](https://cryptotify9.onrender.com/terms.html)
- [ITALIAN ANIMAL ALCHEMY BRAINROT](https://quizverses.github.io/italian-animal-alchemy-brainrot.html)
- [MEME CHALLENGEIO](https://studyplaying.github.io/meme-challengeio.html)
- [CAPYBARA GO](https://quizverses.github.io/capybara-go.html)
- [SUMMER CONNECT](https://quizverses.github.io/summer-connect.html)
- [ROBLO X ZOMBIE](https://quizverses.github.io/roblo-x-zombie.html)
- [NIGHT CLUB SECURITY](https://quizverses.github.io/night-club-security.html)
- [QUIZ X](https://quizverses.github.io/quiz-x.html)
- [CATEGORY CONTROLLER 2](https://quizverses.github.io/category-controller-2.html)
- [HOT COLD WINTER STYLE](https://studyplaying.github.io/hot-cold-winter-style.html)
- [CATEGORY PUZZLE 2](https://studyplayings.pages.dev/category-puzzle-2.html)
- [CATEGORY TRAIN YOUR BRAIN24](https://studyplayings.pages.dev/category-train-your-brain24.html)
- [KAWAII FRIENDS TILES MATCHER](https://quizverses.github.io/kawaii-friends-tiles-matcher.html)
- [CATEGORY COLLECT565](https://quizverses.github.io/category-collect565.html)
- [FIDGET TRADING CARD TOY](https://quizverses.github.io/fidget-trading-card-toy.html)
- [NITRO SPEED CAR RACING](https://studyplaying.github.io/nitro-speed-car-racing.html)
- [UFO ATTACK](https://studyplaying.github.io/ufo-attack.html)
- [CATEGORY FREE RAGDOLL GAMES](https://studyplayings.web.app/category-free-ragdoll-games.html)
- [CATEGORY EXPLOIT](https://thelearnquester.web.app/category-exploit.html)
- [PHOTO BLOCK JOURNEY](https://studyplaying.github.io/photo-block-journey.html)
- [SMASH THE BOTTLE](https://quizverses.github.io/smash-the-bottle.html)
- [TERMS](https://cryptotify.vercel.app/terms.html)
- [TERMS](https://brainquests.netlify.app/terms.html)
- [CATEGORY MAHJONG CONNECT](https://quizverses.github.io/category-mahjong-connect.html)
- [CATEGORY CAT55](https://studyplayings.web.app/category-cat55.html)
- [PRIVACY](https://cryptotify.github.io/privacy.html)
- [RAGDOLL FOOTBALL 2 PLAYERS](https://studyplayings.web.app/ragdoll-football-2-players.html)
- [CATEGORY QUIZ](https://quizverses.github.io/category-quiz.html)
- [WORD JAM ASSOCIATION PUZZLE](https://quizverses.github.io/word-jam-association-puzzle.html)
- [CATEGORY FLASH 2](https://quizverses.pages.dev/category-flash-2.html)
- [TAP TAP BUILDER](https://quizverses-9d2f2.web.app/tap-tap-builder.html)
- [LABUBU DOLL MUKBANG ASMR UNBLOCKED](https://studyplaying.github.io/labubu-doll-mukbang-asmr-unblocked.html)
- [DIAMOND MOSAIC](https://studyplayings.web.app/diamond-mosaic.html)
- [ISLAND PUZZLE BUILD SOLVE](https://quizverses-9d2f2.web.app/island-puzzle-build-solve.html)
- [RUNNING IN FOAM](https://studyplaying.github.io/running-in-foam.html)
- [CATEGORY MAHJONG 2](https://quizverses.pages.dev/category-mahjong-2.html)
- [CATEGORY GUN238](https://thelearnquester.web.app/category-gun238.html)
- [WINTER HEXA STACK](https://studyplayings.web.app/winter-hexa-stack.html)
- [CATEGORY CARE](https://quizverses.pages.dev/category-care.html)
- [LOVE IN STYLE](https://quizverses-9d2f2.web.app/love-in-style.html)
- [CATEGORY ARENA255](https://quizverses.github.io/category-arena255.html)
- [CATEGORY THINKY 3](https://quizverses.github.io/category-thinky-3.html)
- [CATEGORY MAHJONG37](https://quizverses.github.io/category-mahjong37.html)
- [CATEGORY JUMPING147](https://learnquester.github.io/category-jumping147.html)
- [WORM PUZZLE SNAKE APPLE](https://quizverses-9d2f2.web.app/worm-puzzle-snake-apple.html)
- [CATEGORY SPOT THE DIFFERENCE](https://studyplayings.pages.dev/category-spot-the-difference.html)
- [CATEGORY FPS](https://thelearnquester.web.app/category-fps.html)
- [EMOJI GUESS](https://studyplaying.github.io/emoji-guess.html)
- [FARM VS ZOMBIES](https://studyplayings.web.app/farm-vs-zombies.html)
- [SORT WORKS NUTS ORDER](https://quizverses-9d2f2.web.app/sort-works-nuts-order.html)
- [DRAW TO SMASH ZOMBIE](https://studyplaying.github.io/draw-to-smash-zombie.html)
- [CATEGORY BUBBLE SHOOTER](https://quizverses.pages.dev/category-bubble-shooter.html)
- [INDEX20](https://quizverses.pages.dev/index20.html)
- [PURRFECT SCOOPS](https://studyplayings.web.app/purrfect-scoops.html)
- [EVONY THE KINGS RETURN](https://studyplayings.web.app/evony-the-kings-return.html)
- [ARROWTIX TRAIN YOUR BRAIN](https://studyplaying.github.io/arrowtix-train-your-brain.html)
- [CATEGORY AGILITY](https://quizverses.pages.dev/category-agility.html)
- [CATEGORY FREE RAGDOLL GAMES](https://learnquester.github.io/category-free-ragdoll-games.html)
- [CATEGORY PHYSICS371](https://quizverses.pages.dev/category-physics371.html)
- [CATEGORY PUZZLE 5](https://studyplayings.web.app/category-puzzle-5.html)
- [MEDIEVAL ESCAPE](https://quizverses-9d2f2.web.app/medieval-escape.html)
- [CATEGORY SIDE SCROLLING184](https://thelearnquester.web.app/category-side-scrolling184.html)
- [CATEGORY IDLE448](https://thelearnquester.web.app/category-idle448.html)
- [CATEGORY WATER39](https://thelearnquester.web.app/category-water39.html)
- [COLOR MIX JELLY MERGE](https://studyplaying.github.io/color-mix-jelly-merge.html)
- [CATEGORY UNBLOCKED GAMES](https://studyplayings.pages.dev/category-unblocked-games.html)
- [HOSPITAL INC](https://thelearnquester.web.app/hospital-inc.html)
- [SUPERWINGS SUBWAY](https://quizverses.github.io/superwings-subway.html)
- [MOVE EMOJI](https://quizverses.pages.dev/move-emoji.html)
- [CLICKER HEROES](https://quizverses.pages.dev/clicker-heroes.html)
- [CATEGORY SOCCER60](https://thelearnquester.web.app/category-soccer60.html)
- [MALL ANOMALY](https://studyplayings.web.app/mall-anomaly.html)
- [CATEGORY UPGRADE GAMES](https://quizverses.github.io/category-upgrade-games.html)
- [CATEGORY WATER39](https://quizverses.github.io/category-water39.html)
- [CATEGORY BATTLE524](https://quizverses.pages.dev/category-battle524.html)
- [CATEGORY COOKING](https://studyplayings.web.app/category-cooking.html)
- [ORGANIZE IT](https://thelearnquester.web.app/organize-it.html)
- [CATEGORY PUZZLE 11](https://quizverses.github.io/category-puzzle-11.html)
- [CATEGORY OBBY](https://quizverses.pages.dev/category-obby.html)
- [TURBO RACE 3D](https://learnquester.github.io/turbo-race-3d.html)
- [MINIGIANTS IO](https://studyplaying.github.io/minigiants-io.html)
- [REFLECT BEAM LASER LOGIC](https://quizverses.github.io/reflect-beam-laser-logic.html)
- [UNSCREW THEM ALL](https://quizverses.github.io/unscrew-them-all.html)
- [CATEGORY HUNTING16](https://quizverses.github.io/category-hunting16.html)
- [RED STICKMAN VS CRAFTMANS](https://studyplaying.github.io/red-stickman-vs-craftmans.html)
- [CATEGORY SPACE57](https://thelearnquester.web.app/category-space57.html)
- [CATEGORY BUILDING179](https://quizverses.pages.dev/category-building179.html)
- [PET SALON](https://quizverses-9d2f2.web.app/pet-salon.html)
- [FUN IQ PUZZLE](https://studyplaying.github.io/fun-iq-puzzle.html)
- [MY ARCADE CENTER](https://studyplayings.web.app/my-arcade-center.html)
- [CATEGORY CASUAL 3](https://quizverses.github.io/category-casual-3.html)
- [STEAL ITEMS IO](https://quizverses-9d2f2.web.app/steal-items-io.html)
- [IDLE VLOGGER SIMULATOR](https://learnquester.github.io/idle-vlogger-simulator.html)
- [SUPERMARKET SHOPPING FOR KIDS](https://studyplayings.web.app/supermarket-shopping-for-kids.html)
- [IDLE BATHROOM EMPIRE TYCOON](https://studyplayings.web.app/idle-bathroom-empire-tycoon.html)
- [CATEGORY STICKMAN 2](https://thelearnquester.web.app/category-stickman-2.html)
- [CATEGORY CASUAL971](https://quizverses.github.io/category-casual971.html)
- [SOCCER EURO CUP 2025](https://quizverses.github.io/soccer-euro-cup-2025.html)
- [CATEGORY AGILITY 2](https://quizverses.pages.dev/category-agility-2.html)
- [ANOMALY CONTENT RECORD](https://studyplayings.pages.dev/anomaly-content-record.html)
- [FASHION CHALLENGE CATWALK RUN](https://quizverses.github.io/fashion-challenge-catwalk-run.html)
- [IDLE ARCHER TOWER DEFENSE RPG](https://quizverses.github.io/idle-archer-tower-defense-rpg.html)
- [BUS DRIVER SIMULATOR 3D](https://quizverses.pages.dev/bus-driver-simulator-3d.html)
- [CATEGORY SPACE](https://quizverses.github.io/category-space.html)
- [TOW N GO](https://quizverses-9d2f2.web.app/tow-n-go.html)
- [STICK WAR SAGA](https://thelearnquester.web.app/stick-war-saga.html)
- [BITGOBLINS RPG SIMULATOR](https://studyplaying.github.io/bitgoblins-rpg-simulator.html)
- [LOVE TILE TRIO](https://quizverses-9d2f2.web.app/love-tile-trio.html)
