# Changelog

Each release's section is its GitHub release message (the release workflow takes the section of its tag): the
changes to the whole game, then those of single universes, each list with the most important changes first.

## 1.0.11

### Game

- Fixes: HYPERDROP stayed on during the bonus (each piece fell straight onto the stack and couldn't be moved where wanted); it now pauses for the bonus, and a piece already dropped goes back up when the bonus starts

### Universes

- ISLANDS: the camera flies higher and looks down on the sea and the islands
- Fixes: ISLANDS's boats sailing into the islands and drifting sideways (they now sail bow first and steer around the land), ISLANDS's camera going inside the mountains, LIGHTSOURCE's pieces lit as separate cubes (a bar in several shades, a T half in shadow)

## 1.0.10

### Game

- MASTER, a fifth difficulty after EXPERT: every song at the top velocity (40), a 0.3 s lock delay, 5 move resets, EXPERT's other rules; free play starts at level 40; a stage opens on MASTER once done on EXPERT
- NORMAL slightly easier: the Voyage's velocities 10% lower (stage 1 from 4.5, the finale at 20.2), free play's gravity x0.75 (was x0.8)
- Release messages now list the changes as bullet lists

### Universes

- No universe changes in this release

## 1.0.9

### Game

- SAVE SLOTS on the title screen: 3 slots, each with UNLOCK ALL STAGES and ERASE
- Smoother song clock: the fall stays steady at high velocities (no more wobble at velocity 40)
- Web version: installable as an app and playable offline (songs kept once played); runs a frame at a time in the browser; touch pad X button
- A portrait layout for every menu (windows and screens taller than wide)
- Custom voyages: a SHUFFLE SONGS checkbox when naming one; songs over 10 minutes are left out
- Free play: Electro House 2026 first and featured, selected by default
- Voyage: a stage's word follows the wheel's animation (CLEARED when its petal turns, LOCKED until the lock breaks)
- UNLOCK ALL STAGES also opens custom voyages made since
- Campaign: stage 2's FRACTAL song (Elderbrook - Shallow Water) on OBSIDIAN
- Fixes: the game over logo freezing mid-orbit, the space nebula on phones and WebGL

### Universes

- GLACIER reworked: a sculpted blue ice cave, a frosted ice well, pyramid-topped ice cubes, a golden sunburst drop
- FRACTAL: two near-black palettes, OBSIDIAN and EMBERS; smooth petals without stippled rings
- BIOME sea: fish, jellyfish, anglerfish and the submarine as 3D meshes (were flat sprites)
- RUINS: lush 3D bushes and a 3D leaf canopy (were flat cards)
- SHADOW: the projector setup builds the scene up before the light comes on
- NOTEPAD · CHALK: a darker blackboard
- Fixes: ABYSS's falling piece cut at the top when it appears, SMOKE's cleared rows cut flat

## 1.0.8

### Game

- Voyage game over: RETRY SONG, RETRY STAGE or MENU
- Voyage wheel reworked: the focused petal grows, songs coloured only once solved, an unlock animation on a newly opened stage
- A stage done over several runs counts as cleared
- Free play: Electro House 2026 as a built-in playlist
- Slow-motion bonus: pressing down again sets a resting piece down
- Fullscreen fills the monitor the window is on
- Campaign: new songs and universes for stages 2, 6 and 8 (Nu Aspect - Sweet Release in GLACIER, Ordley - imissthetimes in DOCKS)
- Fixes: gamepad remapping screen and pad keys, the web touch pad hiding when a real gamepad is used, Windows (MSVC) build of the longest shaders

### Universes

- New block looks: LIBRARY's 3D books, CORAL REEF's living coral, LOST PANDA's 3D bamboo cubes, ORBITAL STATION's solar arrays, SMOKE's acrylic slabs, DOCKS's night-sheen containers
- Line clears that burst and tumble: CORAL REEF's blocks fly apart in coral chunks, DIORAMA's tumble down and splash into the sea
- ISLANDS in flattened low poly, with pastel low-poly cubes and cleaner water
- GLACIER: cleared ice cubes melt into puddles; the drop's warm sun rays sweep across the cave
- ORBITAL STATION: a cinematic sky (the sun low in the frame, a lens flare, a layered planet limb)
- LOST PANDA: the world muted while the cub is lost, its colours coming back in the drops
- Fixes: SUMI's outlines at inner corners

## 1.0.7

### Game

- A new menu theme, SPACE (the default): a nebula that follows the stages, stars, asteroids, and a camera gliding between screens; the crystal KALEIDOSCOPE stays available in OPTIONS > MENU THEME
- The logo's orbiting pieces as lit, tumbling 3D cubes, anti-aliased; new icons to match
- OPTIONS > MENU MUSIC: ON / OFF
- Shorter options with their answers told in the panel; frosted glass voyage cards
- Fixes: free play without the menu looping on game over, the options panel overlapping its title

### Universes

- LOST PANDA: chamfered bamboo cubes, a stronger drop that turns the scene's hues, red sparkles flying to the cub, no more silhouettes in the subway
- MANGA: a slight paper texture
- Fixes: ISLANDS blocks losing their glass when they land
