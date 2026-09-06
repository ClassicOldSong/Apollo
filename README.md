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
- [IDLE SUPERMARKET TYCOON](https://quizverses.github.io/idle-supermarket-tycoon.html)
- [SAVE THE CROP](https://iskillplay.web.app/save-the-crop.html)
- [CUTE FOLDING PAPER](https://themindskillplayplay.pages.dev/cute-folding-paper.html)
- [CROWD BATTLE GUN RUSH](https://thelearnquesters.pages.dev/crowd-battle-gun-rush.html)
- [STICKMAN FIGHT PRO](https://quizverses-9d2f2.web.app/stickman-fight-pro.html)
- [SHARK CHOMP CHASE](https://studyquests.github.io/shark-chomp-chase.html)
- [UNBLOCK IT ATLANTIS](https://thequizzone.pages.dev/unblock-it-atlantis.html)
- [NUMBER MERGE 10](https://themindplay.github.io/number-merge-10.html)
- [M5 CITY DRIVER](https://themindskillplayplay.pages.dev/m5-city-driver.html)
- [CATEGORY DRIFTING116](https://thelearnquesters.pages.dev/category-drifting116.html)
- [JUICY MATCH 2](https://theskillquest.pages.dev/juicy-match-2.html)
- [67 CLICKER](https://quizverses.github.io/67-clicker.html)
- [CUTE CRAFT LAB](https://quizverses.pages.dev/cute-craft-lab.html)
- [MAKEUP STUDIO GLAM DIVA](https://learnquester.pages.dev/makeup-studio-glam-diva.html)
- [CATEGORY IDLE448](https://studyquests.github.io/category-idle448.html)
- [SOLITAIRE LAMOUR](https://themindplays.pages.dev/solitaire-lamour.html)
- [CLAP CLAP NIGHTMARE](https://iskillquest.pages.dev/clap-clap-nightmare.html)
- [TURBO RACE](https://quizverses-9d2f2.web.app/turbo-race.html)
- [SPIDER ROPE HERO CITY FIGHT](https://learnquester.pages.dev/spider-rope-hero-city-fight.html)
- [OBBY TOILET LINE](https://studyplayings.web.app/obby-toilet-line.html)
- [CREEPY DRESS UP](https://themindskillplayplay.pages.dev/creepy-dress-up.html)
- [CATEGORY ARENA255](https://themindplay.pages.dev/category-arena255.html)
- [XENO DEFENSE PROTOCOL](https://themindskillplayplay.pages.dev/xeno-defense-protocol.html)
- [BOAT GAME RACING SIMULATOR 3D](https://studyplayings.pages.dev/boat-game-racing-simulator-3d.html)
- [GRILL IT ALL](https://iskillquest.pages.dev/grill-it-all.html)
- [SKATE HOOLIGANS](https://theskillquest.pages.dev/skate-hooligans.html)
- [CATEGORY CASUAL971](https://learnquester.pages.dev/category-casual971.html)
- [CRAZY AXE](https://learnquester.pages.dev/crazy-axe.html)
- [DRUNKEN FIGHTERS](https://quizverses-9d2f2.web.app/drunken-fighters.html)
- [FOOTBALL RUSH 3D](https://themindplay.github.io/football-rush-3d.html)
- [CATEGORY MERGE224](https://themindskillplayplay.pages.dev/category-merge224.html)
- [VARIETY MECHA](https://studyplaying.github.io/variety-mecha.html)
- [INDEX3](https://iskillquest.pages.dev/index3.html)
- [RACING FOR TWO ON ONE PC](https://quizverses.pages.dev/racing-for-two-on-one-pc.html)
- [MY DOGY VIRTUAL PET](https://studyplaying.github.io/my-dogy-virtual-pet.html)
- [BRAINROT CLICKER](https://themindskillplayplay.pages.dev/brainrot-clicker.html)
- [2048 MATCH BALLS](https://thequizzone.pages.dev/2048-match-balls.html)
- [FIX DA BRAINROT](https://quizverses.pages.dev/fix-da-brainrot.html)
- [SURVIVAL RACING EXTREME ROAD](https://themindplay.pages.dev/survival-racing-extreme-road.html)
- [CATEGORY AVOID295](https://theskillquest.pages.dev/category-avoid295.html)
- [CATEGORY BATTLE524](https://studyplayings.web.app/category-battle524.html)
- [INDEX17](https://learnquester.pages.dev/index17.html)
- [MAGECLASH IO](https://learnquester.pages.dev/mageclash-io.html)
- [CANDY MONSTER RAFFI](https://skillplay.github.io/candy-monster-raffi.html)
- [CATEGORY PUZZLE](https://iskillquest.pages.dev/category-puzzle.html)
- [PEOPLE PLAYGROUND 3D](https://themindplay.pages.dev/people-playground-3d.html)
- [CATEGORY CARE](https://learnquester.pages.dev/category-care.html)
- [MERGE CAR DEFENSE](https://themindplay.pages.dev/merge-car-defense.html)
- [NUBIK CREATE YOUR PLACE](https://iskillquest.pages.dev/nubik-create-your-place.html)
- [INDEX11](https://themindzone.pages.dev/index11.html)
- [CATEGORY MOUSE1 697](https://quizverses.github.io/category-mouse1-697.html)
- [ARCADE ROPE](https://studyplayings.pages.dev/arcade-rope.html)
- [PERFECT ASMR CLEANING](https://thequizzone.pages.dev/perfect-asmr-cleaning.html)
- [SLICE IT UP](https://themindskillplayplay.pages.dev/slice-it-up.html)
- [BARRY PRISON HIDE AND SEEK](https://themindplay.github.io/barry-prison-hide-and-seek.html)
- [CHARGER CITY DRIVER](https://theskillquest.pages.dev/charger-city-driver.html)
- [SANTA VS SKRITCH](https://themindskillplayplay.pages.dev/santa-vs-skritch.html)
- [BRAT GIRL SUMMER](https://studyplaying.github.io/brat-girl-summer.html)
- [RIOT VILLAGE](https://themindplay.pages.dev/riot-village.html)
- [INSPECTOR CAT](https://iskillquest.pages.dev/inspector-cat.html)
- [CATEGORY CLASSIC97](https://studyplayings.web.app/category-classic97.html)
- [CARS WITH GUNS WASTELAND SHOWDOWN](https://theskillquest.pages.dev/cars-with-guns-wasteland-showdown.html)
- [CATEGORY SPACE57](https://iskillplay.web.app/category-space57.html)
- [WEDNESDAY LIGHT ACADEMIA](https://skillplay.github.io/wednesday-light-academia.html)
- [INDEX20](https://studyplayings.web.app/index20.html)
- [MAGNETIC PULL](https://themindplay.pages.dev/magnetic-pull.html)
- [CATEGORY CASUAL 16](https://iskillplay.web.app/category-casual-16.html)
- [MOJICON GARDEN CONNECT](https://iskillquest.pages.dev/mojicon-garden-connect.html)
- [STACKING MATCH](https://thequizzone.pages.dev/stacking-match.html)
- [2048 RUN GORGEOUS BALLS](https://quizverses.github.io/2048-run-gorgeous-balls.html)
- [CATEGORY 2D1 060](https://studyplayings.web.app/category-2d1-060.html)
- [PENGUIN FARM ICE MERGE](https://skillplay.github.io/penguin-farm-ice-merge.html)
- [MAHJONG CONNECT TILES](https://themindplay.pages.dev/mahjong-connect-tiles.html)
- [CATEGORY AGILITY 2](https://studyplayings.web.app/category-agility-2.html)
- [SURVIVE THE NIGHT](https://quizverses.pages.dev/survive-the-night.html)
- [CHOCOLATE DREAM IDLE FACTORY](https://themindskillplayplay.pages.dev/chocolate-dream-idle-factory.html)
- [NUMBER DOMINATION](https://themindskillplayplay.pages.dev/number-domination.html)
- [CATEGORY LOGIC536](https://iskillplay.web.app/category-logic536.html)
- [OFFICE KNIGHT 3D CASTLE DEFENSE](https://quizverses.github.io/office-knight-3d-castle-defense.html)
- [BLIND BOAT SHOOTING MASTER](https://themindplay.github.io/blind-boat-shooting-master.html)
- [CRUSH IT ALL](https://themindplay.pages.dev/crush-it-all.html)
- [PLANTS VS ZOMBIES WAR](https://studyplayings.web.app/plants-vs-zombies-war.html)
- [CATEGORY SOLITAIRE27](https://learnquester.pages.dev/category-solitaire27.html)
- [CATEGORY ADVENTURE 2](https://studyplayings.pages.dev/category-adventure-2.html)
- [COSMIC DASH](https://quizverses-9d2f2.web.app/cosmic-dash.html)
- [CAPYBARA JUMP](https://themindplay.pages.dev/capybara-jump.html)
- [MAGE AND MONSTERS](https://theskillquest.pages.dev/mage-and-monsters.html)
- [CUT THE GRASS 3D](https://quizverses.github.io/cut-the-grass-3d.html)
- [CONTACT](https://studyplayings.pages.dev/contact.html)
- [JOIN CLASH COLOR BUTTON](https://quizverses.pages.dev/join-clash-color-button.html)
- [FRUIT CANDY MERGE](https://quizverses.github.io/fruit-candy-merge.html)
- [CRICKET CLASH PONG](https://quizverses.pages.dev/cricket-clash-pong.html)
- [BRILLIANT JEWELS](https://skillplay.github.io/brilliant-jewels.html)
- [ARCHER DUNGEON HERO](https://studyplaying.github.io/archer-dungeon-hero.html)
- [SPRING MAGIC ENCHANTED WARDROBE](https://quizverses.pages.dev/spring-magic-enchanted-wardrobe.html)
- [MEGA RAMP CAR STUNTS](https://quizverses.github.io/mega-ramp-car-stunts.html)
- [STRIKE IT](https://themindplay.pages.dev/strike-it.html)
- [CATEGORY PROXY](https://studyplayings.pages.dev/category-proxy.html)
- [CATEGORY CUTE62](https://studyplayings.pages.dev/category-cute62.html)
- [VEGAMIX MATCH 3 VILLAGE](https://iskillquest.pages.dev/vegamix-match-3-village.html)
- [CATEGORY UNBLOCKED](https://learnquester.pages.dev/category-unblocked.html)
- [CANDY CASCADE](https://themindplay.pages.dev/candy-cascade.html)
- [SOLAR SMASH](https://themindskillplayplay.pages.dev/solar-smash.html)
- [DICTATOR SIMULATOR 1984](https://learnquester.pages.dev/dictator-simulator-1984.html)
- [VEGAMIX2 WILD WEST](https://quizverses.pages.dev/vegamix2-wild-west.html)
- [CATEGORY MATCH 3 2](https://studyplayings.pages.dev/category-match-3-2.html)
- [CATEGORY RPG80](https://iskillplay.web.app/category-rpg80.html)
- [CONTACT](https://quizverses.github.io/contact.html)
- [FASHION BATTLE FOR SURVIVAL](https://themindplay.pages.dev/fashion-battle-for-survival.html)
- [KITCHEN STAR](https://iskillquest.pages.dev/kitchen-star.html)
- [MR LONG HAND](https://quizverses-9d2f2.web.app/mr-long-hand.html)
- [CUDDLE MONSTER FUSION](https://themindskillplayplay.pages.dev/cuddle-monster-fusion.html)
- [GOLF MINI](https://iskillquest.pages.dev/golf-mini.html)
- [COINS](https://studyplayings.web.app/coins.html)
- [CATEGORY BATTLE ROYALE](https://quizverses.github.io/category-battle-royale.html)
- [CATEGORY HORROR](https://studyplayings.web.app/category-horror.html)
- [SPRUNKI TORCHES MAZE](https://thequizzone.pages.dev/sprunki-torches-maze.html)
- [EARTH DEFENDER](https://themindplay.pages.dev/earth-defender.html)
- [RED LIGHT GREEN LIGHT](https://thequizzone.pages.dev/red-light-green-light.html)
- [PUZZLE BLOCKS CLASSIC](https://theskillquest.pages.dev/puzzle-blocks-classic.html)
- [JUST DICE RANDOM TOWER DEFENCE](https://themindplay.pages.dev/just-dice-random-tower-defence.html)
- [CATEGORY CASUAL971](https://studyplayings.web.app/category-casual971.html)
- [FISH STORY 4](https://skillplay.github.io/fish-story-4.html)
- [CATEGORY HORROR 2](https://studyplayings.web.app/category-horror-2.html)
- [SUPERHERO DROP AND SAVE](https://themindskillplayplay.pages.dev/superhero-drop-and-save.html)
- [EVERYTHING IS IN PLACE RARE FINDS](https://iskillquest.pages.dev/everything-is-in-place-rare-finds.html)
- [VOLLEY BEAN](https://quizverses-9d2f2.web.app/volley-bean.html)
- [BRAINROT MOB CLASH 3D](https://quizverses-9d2f2.web.app/brainrot-mob-clash-3d.html)
- [DTA BEST THIEF](https://theskillquest.pages.dev/dta-best-thief.html)
- [UNSCREW THEM ALL](https://studyplayings.web.app/unscrew-them-all.html)
