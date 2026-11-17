# The owner's gold-run videos as the reference for the original

Recorded 2026-11-17 (planning step 255). **Text and numbers only**: every picture taken from the
videos was a picture of the original's art, was kept on the maker's PC for the measurement and
deleted afterwards, and nothing of it is, or may ever be, in this repository
(`CLAUDE.md`, THE HARD RULE).

## 1. What the videos are

The owner filmed the original about nine years ago, one gold run per dungeon, and put the runs on
his own channel as a playlist of eight videos (his words, 2026-11-17: "you can literally use the
videos from my youtube channel to check everything you need"). Each shows the whole play of a
dungeon, the end screen with the medal, the portals made and the crystals taken. They are the best
reference there is: the original running, a gold play of every dungeon, in its real time.

| # | video id | contents | length |
|---|---|---|---|
| 1 | `c6qQA6i8C8Q` | chapter 1, dungeons 1-16, golden medal | 4:45 |
| 2 | `M8ZORRN-i_I` | chapter 1, dungeons 17-32, golden medal + boss | 4:49 |
| 3 | `-nd-yTs8Joc` | chapter 2, dungeons 1-16, golden medal | 5:07 |
| 4 | `slBW9WSt7Og` | chapter 2, dungeons 17-32, golden medal + boss | 6:34 |
| 5 | `dKhXRFPr1LM` | chapter 3, dungeons 1-16, golden star | 7:14 |
| 6 | `Hrog-NxlToI` | chapter 3, dungeons 17-32, golden star + boss | 8:12 |
| 7 | `y6rqjVHrN8g` | chapter 4, dungeons 1-16, golden medal | 4:54 |
| 8 | `lJW1m7R6nvo` | chapter 4, dungeons 17-32, golden medal, the final | 7:10 |

(`https://www.youtube.com/watch?v=<id>`.) **Only videos 4 and 6 have been read, and only for 2-17,
3-21 and 3-22.** The other 125 dungeons have not been looked at.

**How they were read.** `yt-dlp` to a local file (outside the repository), `ffmpeg` to frames, a
60 fps decode of one dungeon's segment, the view's scale (256 units = 720 pixels at full size, the
camera clamped at the level's edges and calibrated on landmarks taken from the converted level's
own coordinates), the frame number counted from the segment's start. Accuracy about +-1.5 units in
position (+-5 where a camera scrolls), +-1 frame in time. The inputs of each play were replayed in the
port, and the stances of 3-21 and 3-22 were read a second time by another reader.

## 2. What the footage says about the original's pace

All three dungeons agree.

* **The picture updates 30 times a second**, with a stall now and then (2-17: 365 of 443 gaps are
  2 frames of the 60 fps video, 46 are 4, 24 are 3; 3-21: 492 of 585 are 2; 3-22: 411 of 515). A
  stall is caught up, not lost (a 4-frame gap shows twice the displacement). Nominal 30, mean 26.
* **The physics runs in real time at that rate.** The player's free fall fits 999 and 1042 u/s^2
  (3-21, two edges of the hood, rms 0.3-0.5 px), 1120-1200 (2-17, hood, +-100) and about 1130
  (3-22, +-15 %), so about **1000 u/s^2: 1.1 u per update squared**. The port's world carries
  1.11 u per tick squared at 60 ticks a second: **the same per frame, four times too strong per
  second squared, so a fall takes 1.7-2 times less real time** (2-17's drop of about 150 units:
  0.5 s in the footage, 0.27-0.30 s in the port; 3-21's player drop of 128 units: 20 frames
  against 9-15 ticks; knight B's 125: 41 frames against 22 ticks).
* **What the port has right in real time**: the player's walk (117-128 u/s in the footage, 144 in
  the port: within 10-15 %), a knight's walk (62-63 u/s against 62.4), a shot's flight (199-200 u/s
  against 192), a knight's hold (170 frames for a nominal 3 s, 10 s for 10 s) and the door's
  timer (89 frames against 92 ticks).
* **The ramp of 3-22**: from the stone's first touch to its stop the arm takes 25-27 frames
  (0.42-0.45 s, 12-13 updates), turns 0.69 radian in all, at most 3.2 rad/s. The port turns the
  same arm in 12-13 ticks (0.2 s): **the same per update, twice as fast per second** - the owner's
  "the original was rotating slower".
* **The air brake**: after a walk of 4.1 u per update the player's drift in the air is 1-3 units
  over a fall of 0.5 s (2-17: 0.8 and 0.3 u in the next two updates; 3-21: none in 20 frames).
  A hard stop and a strong brake (0.43 a frame at 30 fps, `min(1, fps / 60 x 0.86)`) fit alike:
  the footage cannot tell them apart, because the key was released within about four frames of
  leaving the edge each time.
* **Unexplained**: knight B of 3-21 falls at 400-440 u/s^2 in the same footage whose player falls
  at 1000 (the speed gain per update keeps rising, so it is not damping); 3-22's stone falls about
  21 units in 0.21 s, about 1000 (750-1200), and the port's stone carries 2000. One measurement each.

**What it means**: `ScriptClock` and `clock.json` (`matched_hz` 60, `physics_step_s` 1/30) make the
port play the original as it runs on a 60 Hz screen, where its world is twice as fast as the clock.
The owner's video is the original at 30. His three reports of 2026-11-15 (2-17, 3-21, 3-22) are
what that difference looks like: a fall that takes half the time leaves half the time to let go
of the arrow, to push the falling knight further, and the ramp turns twice as fast.
`docs/parity-backlog.md` PACE-1 is the row; it is the owner's call.

## 3. The three dungeons

### 2-17 (`level16a`), video 4 (Part 17), gold, 7 portals, 5 of 5 crystals

The play uses the drop: the knight is carried to the left end of platform 1146 (over the start
column), stands there 0.3 s with his centre at the edge (254 +-2), walks left two updates (4.0 and
4.2 u), lets go, falls straight down 0.5 s (drift 1-3 u) and lands at **x about 245, 37 units from
the flame** (closest 34 at the turn), stands there 0.3 s and is not burned. The port with the
level's own air-brake rate (42 Hz; step 252) lands at 244.2-244.6, 36.2-36.6 from the flame:
**the same landing**. The port's window is a key held at most 20 ticks (0.33 s) after the corner;
the footage's hold is about 0.07 s. The footage never shows him inside 34 units of the flame.

### 3-21 (`level20b`), video 6 (Part 21), gold, 12 portals, 5 of 5 crystals

The play: exit portal on the crystal above the 2x2 block, own-point portal, the walk left along
the top platform, **a push of knight B off the platform's left end** (the player stops at the
edge for about six frames, the centre at the edge +-2: 99.6 +-1.5 against the plank's end at 98),
B falls straight down (landing x about 80, 0.68 s; its x is not changed after the first 20 units
of the fall), he walks back right, waits, walks left and steps off the same end (the step 4.4-5.4
u per update; x stays at 88.5 for the 20 frames of the fall, no drift), lands on B's head and
rests to its right at about 95 (B shifts 6 u left), then walks left 21 units in 18 frames and
shoves B 23 units in one update into the pit. The port reproduces R1 (hold left 98-176, a second
hold 215-227: B 82.9 against 80 +-3, the player's column 88.5 against 88.5, rest 94.1 against
about 95) and R4 (the footage's staging) **within 1-5 units**. The recorded route and the gold
play of `tests/routes_gold/level20b.json` are the same shape. **Not reproduced**: the owner's
failure. The port slides the capsule off the corner within 9-12 ticks once its centre is under
about 101 (`probe_offset_px` 3 + the plank's end 98); released before x 100 B lands at 85-89 and a
walk-off lands on it. A data change to `hold.probe_offset_px` (3.0 to 0.5) makes him stand at the
corner but fails 12 route checks (the gold play of 3-21, the step-253 walk-off tests, 3-20, 4-29
and 2-17) and was NOT applied.

### 3-22 (`level21b`), video 6 (Part 22), gold, 6 portals, 5 of 5 crystals

The owner's order: a portal over the first gap and the crystal above (two shots), then the **stone's
portal first, a low exit portal second** (at about (273, 97), 21 units over the arm, the shot from
the lone block under the double block; one shot between them is wasted), the stone falls 21 units onto the
arm's left end (about 4 units per update at the touch), the arm turns to -20 degrees (right end up,
left end on the single block) and **stays there for 6.5 s**, the knight takes the key (offscreen),
**stands on the floor's left edge at about 518.5 +-1.5 (the flat top starts at 518.4) without
moving until it walks to the keyhole 10 s after its hold began (F888)**, and the player drops to
the arm's left end with two portals, walks right up the arm (still at -19 degrees), takes the two
crystals at (490, 146) and (490, 182) and steps off the right end; **the arm swings back to +19
degrees only as he leaves it**. The knight falls into the pit at the end, after it has used the key
(F1128-1136).
**The port with these inputs** (`route_v1`; tick = video frame): the stone is placed at tick 446,
hits the arm at 456, the knight is thrown up at x 512.2 to y 113, takes the key at 471, **lands on
the chamfer of the floor's left end (x 513-514, the chamfer runs from 512 to 518.4 at 26.6 degrees)
and slides off: `minion_fell` on tick 552**. This is the owner's report (SEESAW-1), reproduced. The
capsule body of 3-21 does not stop it (lands at 518.2, 0.2 u short of the flat, creeps back,
falls at 582). A capsule of radius 9.2 (the decoded 9.1) or a brake of 0.15 (decoded 0.3) or a
knight friction of 0.05 makes it land on the flat and stand, and the route wins on tick 1130-1135:
a knife-edge, recorded as a candidate, **not applied**. A second difference: in the port the arm
tips under the player's weight at x 446-475 and he misses the crystal at (490, 146) (4 of 5);
in the footage it does not move until he is off it (SEESAW-2).

## 4. The one-number lever, and what it did once

`game/data/clock.json` `physics_step_s` is what the original's world advances in a frame
(1/30 s) and so `SimRatio` (2 = that over the tick). At 1/60 the port's world would run in real
time like the footage's: gravity a quarter, the carried velocities halved, falls twice as long in
ticks. One replay in the Linux clone, data only (restored byte for byte): 3-22 with the owner's
inputs. With `physics_step_s` 1/60 **no knight falls into the pit** (it creeps along the arm more
slowly, is thrown up and comes back to x about 484 on the arm; `key_picked` at 498), and the
recorded inputs, timed for the other pace, do not win (the player dies at tick 915). So the
knight's death is a thing of the 60 Hz pace, as the arm's speed is. **This is one replay, not a
verdict**: a port-wide change would move every route's timing, every pinned tick, every golden
capture (the owner's files) and the controls census's reading of the recorded plays, and is a step
of its own with the owner's word.

## 5. Open

* The other 125 dungeons: the footage of each gold play is there to be transcribed (the portal
  count and the crystals are on the end screen; the order of shots and the timings are in the
  frames) and replayed in the port, to find every dungeon the port cannot play as the footage does.
* Whether the owner wants the original's 30 fps pace (and so the footage's feel) or the 60 Hz pace
  the port was built to (PACE-1).
* Why knight B and the stone of 3-22 fall at a different rate from the player.
* Whether the original's seesaw (3-22's arm) holds against a walking player (SEESAW-2): the
  footage says it does for the 1.2 s in which he is on its right half.
* The air brake: a clip in which the key is held through the fall would separate a brake from a
  stop.
