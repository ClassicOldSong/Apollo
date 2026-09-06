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
- [CATEGORY LOGIC538](https://quizverses-9d2f2.web.app/category-logic538.html)
- [CARS DERBY ARENA](https://studyplayings.web.app/cars-derby-arena.html)
- [ARCHER LEGEND](https://learnquester.github.io/archer-legend.html)
- [POP THEM](https://skillplay.github.io/pop-them.html)
- [INDEX11](https://studyquesthub.web.app/index11.html)
- [CATEGORY PUZZLE](https://quizverses.pages.dev/category-puzzle.html)
- [ARROW COUNT MASTER](https://learnquester.github.io/arrow-count-master.html)
- [CAT CHAOS SIMULATOR](https://quizverses.pages.dev/cat-chaos-simulator.html)
- [LITTLE BUGS](https://learnquester.pages.dev/little-bugs.html)
- [GET READY WITH ME CONCERT DAY](https://learnquesters.pages.dev/get-ready-with-me-concert-day.html)
- [CATEGORY IDLE](https://learnquester.github.io/category-idle.html)
- [MERGE GUN FPS SHOOTING ZOMBIE](https://learnquesters.pages.dev/merge-gun-fps-shooting-zombie.html)
- [ARROW PUZZLE](https://quizverses.github.io/arrow-puzzle.html)
- [CATEGORY ANIMAL216](https://quizverses.pages.dev/category-animal216.html)
- [KNOCK AND RUN 100 DOORS ESCAPE](https://learnquesters.pages.dev/knock-and-run-100-doors-escape.html)
- [CANDY POP MANIA](https://learnquesters.pages.dev/candy-pop-mania.html)
- [ANIMALS MERGE](https://themindplays.pages.dev/animals-merge.html)
- [EGG ADVENTURE MIRROR WORLD](https://themindplaying.web.app/egg-adventure-mirror-world.html)
- [ZIP ZAP](https://quizverses.github.io/zip-zap.html)
- [MATHEMATICS RACING](https://thelearnquester.web.app/mathematics-racing.html)
- [CATEGORY UNBLOCKED GAMES](https://thelearnquester.web.app/category-unblocked-games.html)
- [CATEGORY EDUCATIONAL](https://studyquests.github.io/category-educational.html)
- [YUMMY TALES 4](https://quizverses.pages.dev/yummy-tales-4.html)
- [FALLING ART RAGDOLL SIMULATOR](https://themindplay.pages.dev/falling-art-ragdoll-simulator.html)
- [CAT CUT](https://quizverses.pages.dev/cat-cut.html)
- [DIGGER FIGHTER IN THE MAZE](https://thelearnquester.web.app/digger-fighter-in-the-maze.html)
- [BACK TO SCHOOL UNIFORMS EDITION](https://quizverses.github.io/back-to-school-uniforms-edition.html)
- [INDEX4](https://thelearnquester.web.app/index4.html)
- [JEWELS BLITZ LEGENDS](https://quizverses-9d2f2.web.app/jewels-blitz-legends.html)
- [CATEGORY IDLE448](https://studyquests.github.io/category-idle448.html)
- [NUTS BOLTS WOOD PUZZLE GAME](https://themindplay.pages.dev/nuts-bolts-wood-puzzle-game.html)
- [CATEGORY PUZZLE 3](https://learnquester.github.io/category-puzzle-3.html)
- [DREAMY HOME](https://themindplay.pages.dev/dreamy-home.html)
- [FLY FLY FLY](https://themindplay.pages.dev/fly-fly-fly.html)
- [NOOB SHOOTER GUN BATTLE 3D](https://studyplayings.pages.dev/noob-shooter-gun-battle-3d.html)
- [LIVE OR DIE](https://thelearnquester.web.app/live-or-die.html)
- [EMOJI CHALLENGE](https://themindplay.pages.dev/emoji-challenge.html)
- [ROYAL JIGSAW](https://themindplay.pages.dev/royal-jigsaw.html)
- [ULTRAHERO VS MONSTERS ROYALE BATTLE](https://quizverses.github.io/ultrahero-vs-monsters-royale-battle.html)
- [DREAM ROOM MAKEOVER](https://themindplay.pages.dev/dream-room-makeover.html)
- [CATEGORY SECURLY](https://studyquests.github.io/category-securly.html)
- [CATEGORY SURVIVAL366](https://quizverses.pages.dev/category-survival366.html)
- [ROMANTIC MATCH TACTICS](https://themindplay.pages.dev/romantic-match-tactics.html)
- [PONGOAL](https://themindplay.pages.dev/pongoal.html)
- [ORGANIZE IT](https://themindplay.pages.dev/organize-it.html)
- [FAMILY TREE PUZZLE](https://themindplay.pages.dev/family-tree-puzzle.html)
- [CATEGORY SNAKE40](https://theskillquest.pages.dev/category-snake40.html)
- [HYPER NURSE HOSPITAL GAMES](https://themindplay.pages.dev/hyper-nurse-hospital-games.html)
- [SKIP LOVE](https://themindplay.pages.dev/skip-love.html)
- [SHOOT N CRUSH](https://themindplay.github.io/shoot-n-crush.html)
- [INDEX17](https://themindplay.pages.dev/index17.html)
- [CATEGORY PUZZLE 5](https://learnquester.github.io/category-puzzle-5.html)
- [CATEGORY CUTE62](https://studyquests.github.io/category-cute62.html)
- [GRILL IT ALL](https://learnquesters.pages.dev/grill-it-all.html)
- [THE BODYGUARD](https://themindplay.pages.dev/the-bodyguard.html)
- [COLOR SORT PUZZLE](https://thequizzone.pages.dev/color-sort-puzzle.html)
- [1010 ELIXIR ALCHEMY](https://iskillquest.pages.dev/1010-elixir-alchemy.html)
- [OFFICE ESCAPE TO DATE](https://learnquesters.pages.dev/office-escape-to-date.html)
- [CATEGORY MATCH THREE](https://studyquests.github.io/category-match-three.html)
- [ZERO TO MILLIONAIRE](https://themindplay.pages.dev/zero-to-millionaire.html)
- [BOLTS AND NUTS](https://thelearnquester.web.app/bolts-and-nuts.html)
- [K POP HUNTER HALLOWEEN FASHION](https://themindplay.pages.dev/k-pop-hunter-halloween-fashion.html)
- [MERGE BALLS NEW YEARS TOYS IN 3D](https://themindplay.pages.dev/merge-balls-new-years-toys-in-3d.html)
- [UPHILL RUSH 13](https://themindplay.github.io/uphill-rush-13.html)
- [WOOD BLOCKS JAM](https://quizverses.github.io/wood-blocks-jam.html)
- [MOSQUITO BITE 3D](https://learnquesters.pages.dev/mosquito-bite-3d.html)
- [ABOUT A FROG](https://studyplayings.pages.dev/about-a-frog.html)
- [HEXA GO](https://quizverses.github.io/hexa-go.html)
- [HIDDEN OBJECT GIRL AND CAT](https://themindplay.pages.dev/hidden-object-girl-and-cat.html)
- [BLOXORZ BLOCK PUZZLE 3D](https://thequizzone.pages.dev/bloxorz-block-puzzle-3d.html)
- [SEEK FIND](https://themindplay.pages.dev/seek-find.html)
- [THE ZOMBIE HOUSE](https://quizverses.github.io/the-zombie-house.html)
- [HIDE AND BUILD A BRIDGE](https://themindplay.pages.dev/hide-and-build-a-bridge.html)
- [BUBBLE SHOOTER BLAST](https://themindplay.pages.dev/bubble-shooter-blast.html)
- [CATEGORY INTERSTELLARUNBLOCKER](https://studyquests.github.io/category-interstellarunblocker.html)
- [CATEGORY FPS 2](https://studyquests.github.io/category-fps-2.html)
- [COMBINE PICKAXES](https://studyplayings.pages.dev/combine-pickaxes.html)
- [MURDER](https://quizverses.github.io/murder.html)
- [CATEGORY ESCAPE 2](https://learnquesters.pages.dev/category-escape-2.html)
- [SAVE THE SHEEP](https://quizverses.pages.dev/save-the-sheep.html)
- [STICKMAN JAILBREAK STORY](https://themindplays.pages.dev/stickman-jailbreak-story.html)
- [BYEPASSHUB](https://thelearnquester.web.app/byepasshub.html)
- [DYNAMONS 10](https://themindplays.pages.dev/dynamons-10.html)
- [BRAINROT MERGE DROP PUZZLES](https://thelearnquester.web.app/brainrot-merge-drop-puzzles.html)
- [DIY MAKEUP SALON SPA MAKEOVER STUDIO](https://thequizzone.pages.dev/diy-makeup-salon-spa-makeover-studio.html)
- [CARS MERGE](https://thequizzone.pages.dev/cars-merge.html)
- [POPCORN STACK](https://themindplay.github.io/popcorn-stack.html)
- [LABUBA MERGE](https://themindplay.github.io/labuba-merge.html)
- [HERO RAGDOLL FIGHTING](https://thequizzone.pages.dev/hero-ragdoll-fighting.html)
- [MOJICON GARDEN CONNECT](https://iskillquest.pages.dev/mojicon-garden-connect.html)
- [CATEGORY CASUAL 10](https://studyquests.github.io/category-casual-10.html)
- [CAT VS GRANNY CAT SIMULATOR](https://learnquesters.pages.dev/cat-vs-granny-cat-simulator.html)
- [MUKI WIZARD](https://themindplays.pages.dev/muki-wizard.html)
- [CLEAN THE FLOOR](https://quizverses-9d2f2.web.app/clean-the-floor.html)
- [FAMILY SIMULATOR BEACH GAMES](https://themindplay.pages.dev/family-simulator-beach-games.html)
- [CONTACT](https://thelearnquester.web.app/contact.html)
- [FRUIT CONNECT 3](https://thequizzone.pages.dev/fruit-connect-3.html)
- [GOLF MINI](https://thequizzone.pages.dev/golf-mini.html)
- [THE OFFICE ESCAPE](https://themindplay.github.io/the-office-escape.html)
- [3D MATCH PUZZLE MANIA](https://theskillquest.pages.dev/3d-match-puzzle-mania.html)
- [SUMMER ONET CONNECT](https://studyplayings.pages.dev/summer-onet-connect.html)
- [SUPER BRAIN](https://quizverses.pages.dev/super-brain.html)
- [MONSTERELLA FANTASY MAKEUP](https://themindplay.pages.dev/monsterella-fantasy-makeup.html)
- [ASTRO KITTY RUSH](https://theskillquest.pages.dev/astro-kitty-rush.html)
- [MATH WALL SIMULATOR](https://iskillquest.pages.dev/math-wall-simulator.html)
- [COLOR BLOCK JAM 2](https://thequizzone.pages.dev/color-block-jam-2.html)
- [GUINEA PIGGY MATCHING](https://themindplay.pages.dev/guinea-piggy-matching.html)
- [CATEGORY CASUAL 12](https://themindplay.pages.dev/category-casual-12.html)
- [MUSHROOM FEVER MATCH 3](https://thequizzone.pages.dev/mushroom-fever-match-3.html)
- [OVERFLOWING PALETTE](https://quizverses.github.io/overflowing-palette.html)
- [CATEGORY CASUAL 9](https://studyquests.github.io/category-casual-9.html)
- [JUMPERS QUEST](https://quizverses-9d2f2.web.app/jumpers-quest.html)
- [COUNTRYSIDE DRIVING QUEST](https://learnquesters.pages.dev/countryside-driving-quest.html)
- [EARWAX CLINIC](https://quizverses.github.io/earwax-clinic.html)
- [TICTOC BRAIDED HAIRSTYLES](https://learnquesters.pages.dev/tictoc-braided-hairstyles.html)
- [CATEGORY ADVENTURE 2](https://studyquests.github.io/category-adventure-2.html)
- [PLANTS VS ZOMBIES WAR](https://thequizzone.pages.dev/plants-vs-zombies-war.html)
- [GEOMETRY ARROW 2](https://quizverses-9d2f2.web.app/geometry-arrow-2.html)
- [ULTIMATE YATZY](https://quizverses.github.io/ultimate-yatzy.html)
- [HOSPITAL INC](https://iskillquest.pages.dev/hospital-inc.html)
- [MERGE FLOWERS](https://themindplays.pages.dev/merge-flowers.html)
- [STICK NINJA SURVIVAL](https://learnquesters.pages.dev/stick-ninja-survival.html)
- [BUILDING MODS FOR MINECRAFT](https://learnquesters.pages.dev/building-mods-for-minecraft.html)
- [HOARD MASTER ONLINE](https://themindplay.pages.dev/hoard-master-online.html)
- [GUESS WORD](https://thequizzone.pages.dev/guess-word.html)
- [MATH OBBY](https://quizverses.github.io/math-obby.html)
- [INDEX7](https://themindplay.pages.dev/index7.html)
- [MONSTER IMPACT](https://themindplay.github.io/monster-impact.html)
- [INDEX15](https://quizverses.pages.dev/index15.html)
- [OCEAN KIDS BACK TO SCHOOL](https://themindplay.github.io/ocean-kids-back-to-school.html)
