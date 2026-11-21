# Wrench Swap (Going Commando)

A mod for [OpenRAC](https://github.com/OpenRAC/OpenRAC) that lets you switch between the Omniwrench 8000, 10000
and 12000 at any time.

By Slim Cognito. For Ratchet & Clank: Going Commando (NTSC-U v1.01).

## Using it

Pause, open **Options**, then **Wrench Swap**, and pick the **Omniwrench** you want. The swap happens when you
leave the menu.

- **Normal game:** The setting unlocks as you find the wrenches: the 10000 on Tabora, the 12000 on Aranos
  Prison.
- **Challenge mode:** All three challenge-mode wrenches can be picked from the start.
- **Cheat Mode (Toggle):** In a New Game, all three wrenches will be available from the start. This can be safely toggled off.

The wrench you pick stays picked from planet to planet, however you may sometimes see the Omniwrench 8000 in Ratchet's hand for a moment upon landing. This currently can't be avoided.

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
