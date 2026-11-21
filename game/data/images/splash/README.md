# The Supersonic Engine's logo

`supersonic-logo.png` is the picture the game shows for two seconds when it starts: the logo of the Supersonic Engine, which Magic Portals Enhanced runs on. `game/sim/Splash.hpp`
says how it is placed and faded; `MagicPortalsLayer` draws it before anything of the game.

| | |
|---|---|
| Size | 2258 x 640, RGB (no alpha), 91,721 bytes |
| sha256 | `d5592a2c7b22c2da5bc97736965e346d5f6628958c4b7f8e7032214a0c38a8c1` (pinned in `tools/ci/check_public.py`, `OWN_PICTURES`) |
| Source | `engine/assets/branding/supersonic-logo.svg` (the engine's own logo, viewBox 1200 x 320), drawn at 2x |
| Ground | `#14171C` (20, 23, 28): the fill of the logo's own panel. The canvas is this colour all round the lockup, and the intro's flat background (`Splash::kGround`) is the same colour, so the picture's edge cannot be seen |
| Lockup | The mark, SUPERSONIC, ENGINE and the rule fill x 160 to 2096 and y 143 to 496: 160 and 161 px of ground to the left and right, so centring the picture centres what is seen |
| Terms | The author's own work, **MIT**, like the engine (`engine/LICENSE`, which covers its branding). It is not the original game's art and not Magic Rampage's |
| Magenta | none |

## Where it comes from

It is the same file the sibling edition, Penumbra and the Castle of Shadows Enhanced, ships for its own intro (its `game/data/images/splash/supersonic-logo.png`, built there on
2026-10-02 from the engine's SVG by the script in that folder's README: an edited copy of the SVG without the faint vertical bands and the panel's rounded corners, with the rule
faded out under the wordmark, drawn by a browser at 2x onto a canvas of the ground colour). Copied here byte for byte.

## Why a picture is committed at all

`tools/ci/check_public.py` refuses every picture in a tracked file (the hard rule of `CLAUDE.md`: nothing of the original game's art is ever committed). This one picture is the
checker's deliberate, narrow exception (`OWN_PICTURES`, planning step 272): by exact name, with this README beside it, no larger than 120 kB and only with the bytes pinned above;
`tools/ci/test_check_public.py` (`OwnPicture`) proves that nothing else is let through by it. The owner must accept the exception.
