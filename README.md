# Wrench Swap (Going Commando)

A mod for [OpenRAC](https://github.com/OpenRAC/OpenRAC) that lets you switch between the Omniwrench 8000, 10000
and 12000 at any time. Only the wrench's model and look change; its damage stays the same.

By Slim Cognito. For Ratchet & Clank: Going Commando, every disc OpenRAC supports: NTSC-U v1.01, NTSC-U v2.00
(Greatest Hits) and PAL (Ratchet & Clank 2: Locked and Loaded).

## Using it

Pause, open **Options**, then **Wrench Swap**, and pick the **Omniwrench** you want. The swap happens when you
leave the menu.

- **Normal game:** the setting unlocks as you find the wrenches: the 10000 on Tabora, the 12000 in the Aranos
  prison. Loading a save also checks the planets you have unlocked (Dobbo for the 10000, Gorn for the 12000).
- **Challenge mode:** all three challenge-mode wrenches can be picked from the start.
- **Cheat Mode** (a switch under the mod in the launcher's Mods tab, off by default): a normal game gets all three
  wrenches from the start too. Turned off again, you go back to the wrenches you have actually found.

The wrench you pick stays picked from planet to planet, and it is kept with your saves.

## Installing

In the OpenRAC launcher's Mods tab, add the mod from this repository's address. The launcher installs the
latest release.

## Building

```sh
python3 tools/fetch_sdk.py          # the OpenRAC mod SDK sdk.lock pins, into .sdk/
cmake -S . -B build -G Ninja
cmake --build build
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/wrench_swap -o dist
```

## Releasing

`.github/workflows/build.yml` is the OpenRAC mod SDK's template workflow. Every push and pull request builds and
packages the mod for every platform. A tag `v<version>` (the manifest's version, e.g. `v0.1.6`) publishes the
archives and `SHA256SUMS` on that release, which is what the launcher installs from.

## License

MIT, see [LICENSE](LICENSE).
