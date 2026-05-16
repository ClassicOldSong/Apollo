# Apollo Ubuntu Fork Maintenance

This repository is maintained as the Ubuntu/Linux release line for Apollo. It is not expected to merge cleanly back into ClassicOldSong/Apollo because this fork carries Linux virtual display, EVDI, PipeWire, and Ubuntu packaging behavior that is outside the parent repository's primary Windows release surface.

## Branch Model

- `linux-port` is the active Ubuntu/Linux integration branch.
- Release tags should be cut from a tested `linux-port` commit.
- Parent Apollo changes should be imported through a review branch, not merged directly into `linux-port`.

## Upstream Tracking Policy

Use the upstream tracking workflow to detect parent changes early:

```text
Actions -> Upstream Apollo Compatibility Check -> Run workflow
```

The workflow fetches `ClassicOldSong/Apollo`, tries a no-commit merge into a temporary branch, and reports one of two outcomes:

- Clean merge candidate: a draft pull request can be opened for Linux validation.
- Conflicts or build failures: the workflow uploads a report and does not merge anything.

No parent update should be merged until these Linux gates are checked:

- Ubuntu build completes.
- Unit tests pass where available.
- `sunshine.service` starts under a GNOME Wayland user session.
- EVDI module loads and an Apollo virtual monitor appears.
- Mutter ScreenCast/PipeWire capture works.
- Moonlight/Artemis can connect and stream with usable input and frame pacing.

## Local Upstream Review

For local review:

```bash
git remote add classic https://github.com/ClassicOldSong/Apollo.git 2>/dev/null || true
git fetch classic
git switch -c review/classic-$(date +%Y%m%d) linux-port
git merge --no-commit --no-ff classic/master
```

If conflicts appear, resolve them on the review branch and run the Ubuntu validation gates before merging into `linux-port`.

Abort a local test merge:

```bash
git merge --abort
```

## Ubuntu Release Checklist

Before tagging a release:

```bash
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
cpack -G DEB --config build/CPackConfig.cmake
```

Then install the generated `.deb` on a GNOME Wayland Ubuntu host and verify:

- `evdi` is loaded.
- `systemctl --user status sunshine.service` is active.
- `curl http://localhost:47989/serverinfo` returns server info.
- A Moonlight client can start and stop a virtual display session.
- The physical display is restored after disconnect.

## Flatpak Notes

Flatpak is not the primary release path yet because EVDI is a host kernel module. The Flatpak can be useful once the host has EVDI installed, but Ubuntu `.deb` releases are the supported path for the virtual display backend.
