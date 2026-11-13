# Refall

A music-reactive falling-block puzzle game that plays **any music you like**: your own folder or a YouTube
playlist. Each song is analysed ahead of time (tempo, key, intro, verse, build, chorus, drop...) and the game stages
it: pieces fall on the beat, the pace follows the song's energy, and each song plays in its own **universe**.

```sh
uvx refall            # the title screen: CAMPAIGN or FREE PLAY (the default playlist)
uvx refall ~/Music    # free play on any folders, files and YouTube URLs
```

## Universes

Each universe is a hand-directed world with its own palette, materials, typography and way of staging the music:
calm parts stay quiet, drops and choruses bring the whole picture. Each one comes in several variants picked per
song, and between songs the game takes a short break into the next universe. Fifty universes so far, each with its status:
✅ <i>ok</i> (reviewed and tested) or 🔍 <i>to review</i> (new, still to be played and tested):

<table>
<tr>
<td width="50%"><img src="https://raw.githubusercontent.com/Gregwar/zentris/becac93004878065dbdb2eec1a5f3ea6a91e9ded/docs/universes/vector.jpg"><br><b>VECTOR</b>: phosphor beams on a dark CRT<br><sub>✅ <i>ok</i></sub></td>
<td width="50%"><img src="https://raw.githubusercontent.com/Gregwar/zentris/becac93004878065dbdb2eec1a5f3ea6a91e9ded/docs/universes/sumi.jpg"><br><b>SUMI</b>: ink on washi paper<br><sub>✅ <i>ok</i></sub></td>
</tr>
<tr>
<td><img src="https://raw.githubusercontent.com/Gregwar/zentris/becac93004878065dbdb2eec1a5f3ea6a91e9ded/docs/universes/diorama.jpg"><br><b>DIORAMA</b>: a sunlit clay island<br><sub>✅ <i>ok</i></sub></td>
<td><img src="https://raw.githubusercontent.com/Gregwar/zentris/becac93004878065dbdb2eec1a5f3ea6a91e9ded/docs/universes/abyss.jpg"><br><b>ABYSS</b>: the deep sea, lit by living things<br><sub>✅ <i>ok</i></sub></td>
</tr>
<tr>
<td><img src="https://raw.githubusercontent.com/Gregwar/zentris/becac93004878065dbdb2eec1a5f3ea6a91e9ded/docs/universes/smoke.jpg"><br><b>SMOKE</b>: a dark stage, slow smoke and one hard spotlight<br><sub>✅ <i>ok</i></sub></td>
<td><img src="https://raw.githubusercontent.com/Gregwar/zentris/becac93004878065dbdb2eec1a5f3ea6a91e9ded/docs/universes/watercolor.jpg"><br><b>WATERCOLOR</b>: transparent washes on cold-press paper<br><sub>✅ <i>ok</i></sub></td>
</tr>
<tr>
<td><img src="https://raw.githubusercontent.com/Gregwar/zentris/becac93004878065dbdb2eec1a5f3ea6a91e9ded/docs/universes/polar.jpg"><br><b>POLAR NIGHT</b>: a snowfield under the aurora<br><sub>✅ <i>ok</i></sub></td>
<td><img src="https://raw.githubusercontent.com/Gregwar/zentris/becac93004878065dbdb2eec1a5f3ea6a91e9ded/docs/universes/risograph.jpg"><br><b>RISOGRAPH</b>: a two-ink risograph print<br><sub>✅ <i>ok</i></sub></td>
</tr>
<tr>
<td><img src="https://raw.githubusercontent.com/Gregwar/zentris/049bab4ddf4aea7d90c768546e006132082e7ead/docs/universes/white_room.jpg"><br><b>WHITE ROOM</b>: a seamless white studio, matte blocks and soft shadows<br><sub>✅ <i>ok</i></sub></td>
<td><img src="https://raw.githubusercontent.com/Gregwar/zentris/becac93004878065dbdb2eec1a5f3ea6a91e9ded/docs/universes/kintsugi.jpg"><br><b>KINTSUGI</b>: dark polished stone, ceramic mended with gold<br><sub>✅ <i>ok</i></sub></td>
</tr>
<tr>
<td><img src="https://raw.githubusercontent.com/Gregwar/zentris/becac93004878065dbdb2eec1a5f3ea6a91e9ded/docs/universes/laser.jpg"><br><b>LASER SHOW</b>: every piece projected by its own laser<br><sub>✅ <i>ok</i></sub></td>
<td><img src="https://raw.githubusercontent.com/Gregwar/zentris/210013d9eb38de1166e8753be86cea34904be746/docs/universes/garden.jpg"><br><b>GARDEN</b>: tiles of earth growing pixel grass and flowers<br><sub>✅ <i>ok</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/biome.jpg"><br><b>BIOME</b>: glass cubes of water or seeds bringing a 3D world to life: a sea rising over dry sand, or a jungle growing on parched clay<br><sub>✅ <i>ok</i></sub></td>
<td><img src="https://raw.githubusercontent.com/Gregwar/zentris/8d4d510ae0b93552a83ed57d0608ba5dadbda54b/docs/universes/fluo.jpg"><br><b>FLUO</b>: fluorescent paint splashed on a dark wall, glowing under a blacklight on drops<br><sub>✅ <i>ok</i></sub></td>
</tr>
<tr>
<td><img src="https://raw.githubusercontent.com/Gregwar/zentris/8d4d510ae0b93552a83ed57d0608ba5dadbda54b/docs/universes/marbles.jpg"><br><b>MARBLES</b>: glass marbles in square tiles, shot out across a dark table where they collide and stain the felt<br><sub>✅ <i>ok</i></sub></td>
<td><img src="https://raw.githubusercontent.com/Gregwar/zentris/412230bfad663a226675f239505670847baf2334/docs/universes/prism.jpg"><br><b>PRISM</b>: glass cubes in the dark, slow beams of light, a rainbow on the drop<br><sub>✅ <i>ok</i></sub></td>
</tr>
<tr>
<td><img src="https://raw.githubusercontent.com/Gregwar/zentris/cb76251de16f7e81e5f2641e610ecb2f19a82a2e/docs/universes/lightsource.jpg"><br><b>LIGHTSOURCE</b>: a dim room lit by the falling piece, the stack casting its shadows<br><sub>✅ <i>ok</i></sub></td>
<td><img src="docs/universes/notepad.jpg"><br><b>NOTEPAD</b>: a squared notebook drawn by hand in stop motion, erased and redrawn<br><sub>✅ <i>ok</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/city.jpg"><br><b>CITY</b>: a tower in a dark 3D night city, its lit rooms the blocks<br><sub>✅ <i>ok</i></sub></td>
<td><img src="docs/universes/topography.jpg"><br><b>TOPOGRAPHY</b>: a contour map whose terrain is the stack, a slowly shifting land; rivers in the rain, a flood and living peaks on the drop<br><sub>✅ <i>ok</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/heat.jpg"><br><b>HEAT</b>: a thermal camera; pieces land white hot and cool down, the heat conducts through the stack and rises, the drop flares into another colormap<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/blueprint.jpg"><br><b>BLUEPRINT</b>: a technical drawing of hatched, dimensioned parts; a machine drawn in the margins comes alive on the drop<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/recycle.jpg"><br><b>RECYCLE</b>: a table seen from above, the pieces assembled from heaps of cubes beside the well and cleared rows recycled into them<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/electron.jpg"><br><b>ELECTRON</b>: solid cubes on a dark substrate, wired up by roaming charges whose glowing connections emerge between two terminals<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/lanterns.jpg"><br><b>LANTERNS</b>: a 3D night lake of paper lanterns; each cleared row slides out of the well and floats onto the water as lanterns that stay lit, the drop lifts them all into the sky<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/origami.jpg"><br><b>ORIGAMI</b>: folded paper pieces on a sheet; each cleared row slides out of the well and is folded into a paper crane step by step, the cranes gather around the well and the drop sets the flock flying<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/rainy.jpg"><br><b>RAINY WINDOW</b>: a night window in the rain, the city out of focus behind it; drops run around the stained glass pieces on the beat, the drop turns the city's lights<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/bricks.jpg"><br><b>BRICKS</b>: glossy toy bricks in a photo studio; each cleared row flies off as bricks that build models on turntables, the drop takes them apart and rebuilds them<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/crystal.jpg"><br><b>CRYSTAL CAVE</b>: gemstone pieces in a geode cave; each phase of the song is its own mineral, frozen into the stack at lock time, so the stack grows in strata that record the song<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/swiss.jpg"><br><b>SWISS POSTER</b>: a Swiss typographic poster set from the song's words, recomposed on the bar grid; flat pieces lock in each section's palette, the drop cuts to the negative print, the title giant<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/limbo.jpg"><br><b>LIMBO</b>: black silhouettes in a luminous fog; each cleared row adds something to the landscape (birds, trees, lanterns, distant towers), the drop parts the fog and turns the scene to negative<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/neon.jpg"><br><b>NEON</b>: glowing watercolor shapes swept in a spectrum over a painted night skyline; each piece keeps its section's colors, each cleared row paints a building into the city, the drop washes the skyline in the spectrum<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/microscope.jpg"><br><b>MICROSCOPE</b>: a Petri dish through the eyepiece, the pieces stained plant cells, some with a microorganism living inside; a cleared row frees them to swim in the dish for the rest of the song, the drop washes in a new stain<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/sonar.jpg"><br><b>SONAR</b>: a glide through a dark 3D sunken city where shapes glow up from the black as echoes return on the beat; each cleared row charts the seabed for good, the drop lights the whole world and the glide surges<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/forest.jpg"><br><b>GLOWING FOREST</b>: a night forest lit only by bioluminescence, trunks fading into the mist, a mossy rock and a still pool; glowing fungus cubes, each cleared row sows a glowing plant that stays, plants glow on the beat grid and the drop sends a slow wave of light through the forest<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/docks.jpg"><br><b>DOCKS</b>: a container port at dusk in 3D; the falling container piece hangs from a gantry on hairline cables, cleared rows are craned onto ships that fill up over the song, the drop switches the port to full light<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/bauhaus.jpg"><br><b>BAUHAUS</b>: a Bauhaus poster of circles, triangles and squares cut out of thick colored paper and laid on the sheet in relief (soft shadows, lit edges), recomposed on the bar grid; each cleared row joins the poster as a small cut-out in a growing sampler, the drop lifts the forms off the sheet into a turning kinetic sculpture under a circling spotlight, then folds them back into a new composition<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/skyocean.jpg"><br><b>CELESTIAL OCEAN</b>: a sea of clouds at night under a big moon; moonstone pieces, each cleared row rises out of the clouds as a translucent creature of light (a school of fish, jellyfish, a manta ray, a whale for a quad) swimming away through the starry sky, the drop brings a giant whale slowly across the whole sky<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/manga.jpg"><br><b>MANGA</b>: a manga color page in ink, screentones and watercolor washes, drawn in pencil, then inked and colored panel by panel as rows clear; the pieces are inked blocks of color under their screentones, the drop is a full-page splash of color, focus lines and a huge sound effect<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/brutalist.jpg"><br><b>BRUTALIST</b>: raw board-marked concrete under a hard sun in 3D; the pieces are cast blocks in a slot cut into a monument, the sun moves on with the song and its shadows sweep the plaza, each cleared row has a crane lower a module onto a growing building, the drop is the sun breaking through<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/tiles.jpg"><br><b>TILES</b>: a great wall mosaic of tesserae laid in rows following its figures (andamento), gold, glass and stone glinting under a moving lamp; the pieces are glazed azulejo-like tiles, each cleared row breaks into chips tapped one by one into the figures sketched on the bare bed, the drop sets the smalti ablaze<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/gianttree.jpg"><br><b>GIANT TREE</b>: a toon world tree shooting up absurdly with every cleared line, from a round sapling in a bright meadow through the clouds, the stratosphere and orbit until the planet is a ball under its roots and its crown passes the moon; the drop beams a rainbow up the trunk into space<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/mycelium.jpg"><br><b>MYCELIUM</b>: a glowing fungal network grows over the forest floor at night with the cleared lines, from a single spore among the moss (mushrooms bursting up into glowing caps) through a clearing, the forest, a valley with a village and a river, the clouds, a region where the threads glow under the land like veins, until the Earth seen from space on its night side is wrapped in a living web of light, the moon beside it; each clear spurts it outward with a pulse of light from the spore, the drop sends a wave of light through the whole network, up every mushroom's stem, and clouds of spores into the night<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/megacity.jpg"><br><b>MEGACITY</b>: a hut on a riverbank at dusk sprawling with every cleared line along its river, highways and bay into a village, a walled town, a long night skyline of towers and megatalls, arcologies and a space elevator, until the planet is one glowing ball of city lights under an orbital ring; the drop raises a whole district whose lights come on block by block on the beat<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/orbital.jpg"><br><b>ORBITAL STATION</b>: a space station building itself in orbit and growing absurdly, a stage an order of magnitude bigger: from a lone capsule to a habitat wheel, a forest ring, nested rings, shipyards, a space elevator, a ring round the planet, more rings and elevators, an O'Neill fleet, the Moon taken in hand, and two rings dwarfing the planet; tugs and traffic never stop; the drop spins the rings up and chases light round them on the beat<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/library.jpg"><br><b>LIBRARY</b>: the blocks are clean book-bound cells; each cleared line sends three big books, behind the board, to a cosy toon library room seen as a diorama (in the spirit of Link's Awakening: cut-away walls, round windows, a reading nook, a big plant) that fills over one song: the bookcases, a ladder, piles on the floor, a mezzanine; on the drop the books breathe on their shelves and loose pages flutter<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/skywhale.jpg"><br><b>SKY WHALE</b>: a little koi in a lily pond at dusk, growing with every cleared line, a few strokes of its tail each time: it slips down the river to the sea, breaches as a whale by a lighthouse, swims up through the clouds with seabirds and balloons in its wake, glides around the Earth trailing aurora from its fins, then heads out to the Moon and glides low over its craters; on the drop it sings, soft rings of light rippling out through the sky<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/planks.jpg"><br><b>PLANKS</b>: wooden plank blocks in a white studio, each cell a solid block of three stained planks; each cleared row's planks slip out behind the well, Jenga style, to build a plank village beside it (a tower, a temple, a train on its track, a pagoda), the train shuttles at a peak and the village topples at game over<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/tidepool.jpg"><br><b>TIDE POOL</b>: the whole universe, and every cleared line zooms deeper, one continuous dive: a galaxy at a node of the cosmic web, a star in its arm, a planet, its coast, the shore and a humble rock pool, then the floor's anemones and limpets, plankton in the water, a diatom's glass rose window, a living cell, then imagined physics (molecules, breathing orbital clouds, a boiling quantum foam, strings of light ringing with the song) until the universe opens out of them again; the pieces are sea glass<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/lostpanda.jpg"><br><b>LOST PANDA</b>: a little kawaii panda, lost in the city at night under the rain, finds its way home one cleared line at a time, in one continuous shot through a world cut away like a cross-section: the shopping street, down the stairs into the subway, a train through the tunnel and out at dawn, the industrial district, the fields, the bamboo forest; fed by bamboo snacks from each clear it grows stronger, and at the end it runs across the screen to its family; the blocks are bamboo segments<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/terrarium.jpg"><br><b>TERRARIUM</b>: a sealed glass jar on a windowsill whose little world grows with the cleared lines into a miniature jungle; then a second jar is set down beside it and comes alive in turn (a flowering meadow, a snail), then a pond jar, a desert jar and a few more, linked by glass tubes, until a small cluster of terrariums glows at night with fireflies in every jar and along the tubes; the light outside goes from day to night, a kawaii ladybird visits the next jar through its tube, the drop is a summer shower, and the blocks are polished stones<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/coralreef.jpg"><br><b>CORAL REEF</b>: a sunny reef from bare rock and white sand to a dense coral garden over one song: every cleared row bursts into bubbles and drifting seeds that plant brain corals, staghorns, sea fans, sponges and table corals behind the well, while schools of fish follow the melody; caustics pulse with the kicks, a manta glides past on the drop, slow motion turns the water deep blue; the blocks are pale sun-bleached coral<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/woodstock.jpg"><br><b>WOODSTOCK</b>: a late-60s psychedelic cartoon in thick white outlines: a kaleidoscope breathing with the beat around a calm dark well, flat candy blocks each with a small daisy, heart, peace sign, sparkle, spiral, rainbow or kawaii face; cleared rows burst into flowers, hearts and peace signs, and a garden of daisies and mandala flowers, a rainbow, a smiling sun, mushrooms and doves grows at the sides over one song; the drop folds the kaleidoscope into a mandala and swings the palette<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/architect.jpg"><br><b>ARCHITECT</b>: a modern villa built over one song on an empty green field: every cleared row is a delivery, a tower crane lowering the next room, glazing or roof into place with the load swinging softly on its cables; the pool is dug, tiled and filled, the garden planted, the cranes leave, and at dusk the rooms light up and the pool glows; the drop switches the site's floodlights on; the blocks are pigmented architectural concrete with tie holes<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/islands.jpg"><br><b>ISLANDS</b>: one continuous flight over an endless tropical archipelago, like drone footage: volcanic islands with cliffs and waterfalls, sandy cays, atolls, a lighthouse islet, bungalows on stilts, a white villa with its pool and deck, turquoise shallows, sailboats and speedboats with their wakes; calm parts fly high and wide, now and then above the clouds, peaks skim the waves; every cleared row launches a sailboat with sails in the row's colors, a fleet that grows over the song and lights its lanterns at dusk; the drop dives to the waves into a regatta of coloured spinnakers, golden light and leaping dolphins (fireworks at night); the day passes from midday to dusk; the blocks are painted driftwood boards on a translucent pane of deep water<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/rain.jpg"><br><b>RAIN</b>: rain everywhere over a colorful street at dusk (neon signs, a flower market or a monsoon's lanterns, all mirrored in the puddles); the rain pours on the stack with its own physics, splashing and running down the wet pastel glazed tiles, the falling piece sheltering the cells under it; cleared rows are thrown out to the sides where they tumble into messy heaps (rigid-body physics) under the same rain; the drop is a thunderstorm<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/pinkvibe.jpg"><br><b>PINK VIBE</b>: a soft, girly pastel world in flat design: wide bands of pinks, from blush through rose and salmon to raspberry, wave slowly across the screen and breathe with the music; soft rounded pastel pink blocks, seven distinct pinks each with a tiny calm motif (pearls, a heart, a flower, a sparkle, a crescent, a bow, a bubble) on a translucent plum well; cleared rows drift away as petals and bubbles; the drop swirls the bands into one big wave in rose-gold and magenta sunset tones<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/shadow.jpg"><br><b>SHADOW</b>: the game is a shadow: a little wooden model of the well with glass blocks stands on a table at the bottom left, lit by a vintage spotlight, and the playfield is its shadow cast on the wall, each glass block a pool of colored light; cleared blocks are lifted out of the model and stacked on the table; the drop turns on a second colored lamp that doubles the shadows<br><sub>🔍 <i>to review</i></sub></td>
</tr>
<tr>
<td><img src="docs/universes/mirror.jpg"><br><b>MIRROR</b>: glowing glass cubes light a dark liminal room (an infinity cove, a step, a pillar), their colors pooling on the glossy floor; a gallery of frameless mirrors floats in the side bands, each reflecting the stack from its own angle and bouncing its light into the room; a cleared row shatters into glinting shards, the drop blooms the light and gathers the mirrors into turning kaleidoscope rings<br><sub>🔍 <i>to review</i></sub></td>
<td><img src="docs/universes/ruins.jpg"><br><b>RUINS</b>: time runs backwards over one song, in real 3D: from a dense jungle that recedes, ruins appear and repair themselves (fallen column tops flying back onto their stumps, stones re-forming from golden dust, towers regaining their tiers) until a Maya, Khmer or Greek temple stands painted in its prime, braziers lit; the blocks are carved stone column drums, and the drop turns the sun to golden hour<br><sub>🔍 <i>to review</i></sub></td>
</tr>
</table>

Disabled for now (not built; their code stays in the repository):

| Universe | Status |
|---|---|
| PIXEL | Too close to a classic falling-block look |
| SYNTHWAVE | To be redesigned (minimal neon) |
| CLOUDSCAPE | To be reworked (nights only) |
| MAGNETS | Iron filings on paper; not convincing yet, waiting for a better idea |
| GARDEN (smooth) | Replaced by its 16-bit version, now GARDEN (`src/universes/garden16/`) |
| SUNPRINT | A sheet turning between two tints, keeping the old one under the stack; to be judged later |
| EMBROIDERY | Cross-stitch on aida; to be judged later |
| SUSHI | Maki in a bento box fed to kawaii cats; on hold |
| RIPPLES | Cut stone blocks in still water seen from above, rippled by every move; on hold |
| STAR CHART | An engraved celestial map filling with constellations; replaced by CELESTIAL OCEAN |
| STAINED GLASS | A cathedral window glazed by molten glass from the cleared rows; disabled |
| GROCERIES | A checkout counter seen from above, cleared rows packed into bags; disabled |
| MUSIC BOX | A cylinder music box pinned with the cleared rows; disabled, not convincing yet |

More in [docs/universes.md](docs/universes.md): every universe and its variants, the transitions, and the
roadmap of the universes to come. To make a new one, see [docs/adding-a-universe.md](docs/adding-a-universe.md).

## Modes

Started with no songs, the game opens on its menu, a stained-glass kaleidoscope (the game is called **Refall** for
now: `GAME_TITLE` in `src/menu.hpp`, shown on the title screen and as the window title): **VOYAGE**, **FREE PLAY** or
QUIT (arrows or D-pad, Enter / A to choose, Esc / B to go back). The game's own universes are folded into the
kaleidoscope: on the title and in free play they turn in turn, on Voyage it shows the most colourful of the focused stage's
universes (the focused song's in an open petal), graded toward the stage's hue: a saturation boost, a contrast stretch
from the snapshot's own luminance range and a duotone between a deep and a bright shade of the stage colour (greyed and
dimmed for a locked stage); free play takes the
focused playlist's hue. They come from snapshots of each universe cached in `~/.cache/zentris/menu-worlds/` (640x360);
the missing ones are made one at a time on a worker thread with its own OpenGL context, only while no key has been
pressed for a moment (the first launch fills the cache in about a minute on the title), and until a snapshot is ready
its place is procedural glass in the universe's colours, cross-fading to the image when it arrives. The menu remembers
where it was (`menu-state.tsv` next to the progress: the title entry, Voyage's difficulty and focused stage, free play's
difficulty and playlist) and opens there on the next launch (`--reset` forgets it). The menu has its own music,
[`data/menu_music.mp3`](data/menu_music.mp3), looping seamlessly (a 1 s crossfade at its loop point) at about 70% of a
game song's level: it fades in when the menu appears, fades out (0.8 s) when a game starts and starts over from the top
back on the menu (`--no-menu-music` keeps the menu silent; `--mute` silences everything; offline renders never play it).
The file is found in the sources' `data/`, else next to the executable (installed builds ship it there). The menu plays
no song previews.

- **Free play** is the game as it always was: the playlist in a random order, a universe picked for each song, levels
  rising with the lines. Choose a difficulty: EASY, NORMAL or EXPERT, a multiplier on the gravity of every level
  (x0.6, x0.8, x1: EXPERT is the full speed), and a **playlist**: down to the PLAYLIST row of glass cards, the game's
  own first (featured: larger, a gold rim), then the ones used before (most recent first), each with its own hue and
  small rosette drawn from its playlist ID; **+ ADD A PLAYLIST** opens a field where a YouTube playlist link, a bare
  playlist ID (`PL...`) or a single video link is typed or pasted (Ctrl+V); Enter reads it (its song count, and its
  title through yt-dlp) and keeps it. Delete forgets the selected one. They are remembered in `playlists.tsv` in the
  user data folder (url, title, songs, last use; `--playlists FILE` for another file). A game over holds until a key
  is pressed, then goes back to the menu.
- **Voyage** (the campaign) is 15 stages of 4 songs played in a row. There are no levels: each song has its velocity
  (a game level, fixed for the whole song) and its universe set by the voyage, higher with each stage and song, and
  with the difficulty (EASY, NORMAL, EXPERT, chosen at the top of the Voyage screen). The stages are 15 glass petals in
  a rosette, numbered 1 to 15, each in its own hue, a gold arc with arrows showing the order and the next stage to clear glowing. Left /
  right moves between the petals (the rosette turns the focused one to the right, where its songs are listed), up
  reaches the difficulty. **Enter plays the whole stage** from its first song; **Space** (or Tab, gamepad X / Y) opens
  the petal, fanning out its songs: left / right chooses one, Enter plays the stage from that song to its last
  one, Esc or Space closes the petal. A locked petal can be focused, not played. Surviving all the songs of a run
  started from the first one **clears** the stage and unlocks the next one (a stage cleared on any difficulty unlocks
  the next). A run started from a later song is fine for practice: the songs it survives count as beaten (with their
  best score) but the stage isn't cleared. A game over ends the run; a key goes back to the stages. The game remembers,
  per difficulty, the stages cleared (with the run's score and lines) and the songs beaten (with the points scored and
  lines cleared during each, and the most lines ever cleared during it), shown in the Voyage's song list, stage header
  and info panel (`-` for lines recorded before they were kept), in `campaign-progress.tsv` in the user data folder
  (`~/.local/share/zentris/` on Linux). The file keys songs by their video id and stage clears by their songs, so
  regrouping the Voyage keeps the songs beaten: a clear of a stage that no longer exists is dropped, and the stages it
  had opened (and every stage with a beaten song) stay open. A file from the first Voyage (12 stages of 5, keyed by
  index) is migrated the same way on load, the old file kept as `campaign-progress.tsv.bak`.

A game over muffles the song like the slow-motion bonus does (a low-pass filter) and lowers its volume, then fades it out
to silence; the game over holds until the player presses a key (with songs given on the command line, a key plays
again).

| Option | |
|---|---|
| `--free` | free play straight away, without the menu |
| `--difficulty easy\|normal\|expert` | free play's difficulty (default: expert), or a gravity multiplier such as `0.7`; also Voyage's |
| `--stage S[:SONG]` | a Voyage run straight away (stage S, from its song SONG) |
| `--unlock` | unlock every Voyage stage (saved) |
| `--reset` | forget the Voyage progress (prints what was forgotten) |

Songs or playlists given on the command line play free play straight away, as before; so do the offline renders
(`--review-shot`, `--clip`, `--analysis`, ...).

Without a display, `--menu-shot OUT.png [--menu-screen title|campaign|free] [--menu-hover S:SONG] [--menu-petal]`
saves a menu screen (`--menu-petal`: the hovered stage's petal open; `ZEN_TEST_KEYS` presses keys before it) and
`--menu-clip OUT.mp4` records a 34 s walk through the menu (needs ffmpeg; `ZEN_MENU_CLIP_SECONDS` changes its
length), printing the menu's average CPU and GPU time per frame; both can show made-up progress and playlists
(`--menu-demo`, always on in clips, never saved; `--menu-demo normal-done`: every stage cleared on NORMAL, the first
five on EASY, none on EXPERT). The menu's font, Josefin Sans, is OFL, in `src/embed/` with its licence.

### The Voyage file

The campaign is [`data/campaign.tsv`](data/campaign.tsv), one song per line, tab-separated (`#` starts a comment):

```
# stage  song  id           universe             easy  normal  expert  title
1        1     H_9xqc084UA  PRISM:AURORA         1.0   2.0     4.0     Small Town Kid - Shards
15       4     9Sh9ZSecyFU  SKY WHALE:MOONLIGHT  8.2   13.5    20.0    callah x Laura Brehm - skyline
```

- `stage`, `song`: the stage (1..) and the song's place in it (1..); any number of stages and songs works.
- `id`: the YouTube video id (songs are downloaded and cached like the playlist's).
- `universe`: `NAME` or `NAME:VARIANT` as `--universe` takes it (empty: picked for the song as in free play).
- `easy`, `normal`, `expert`: the song's velocity on each difficulty, as a game level from 1 (the first level) to 60
  (20 is the free play's plateau, x10 gravity; past it faster still, up to the gravity's cap); decimals are fine.
- `title`: shown in the menu and the title card.

The game reads the file from the sources when it starts (or `--campaign FILE`), else from next to the executable,
else the copy built into it. The shipped Voyage tours 60 universes once each (SKY WHALE in its MOONLIGHT variant
ends stage 15) on songs of the default playlist. Each stage builds to a climax: its last song is much faster than its first (on
EXPERT, 20 levels faster on stage 1, 35 on the finale), over a base that rises across the stages (EXPERT goes from 6 to
26 on stage 1 and from 25 to 60 on the finale, far past the free play's plateau; NORMAL tops out at 28, EASY at 13.5).
On EXPERT, from velocity 45 on, the drops go 20G (a piece falls to the stack the moment it appears, with a longer lock
delay to slide and turn it along the stack; at velocity 60 the choruses too), the rest of the song keeping a visible
fall: in the last song of stages 10 to 12, the last two of stages 13 and 14, and every song of the finale.
A song's velocity is not a constant: the run's first song starts as a chill warm-up (a third of its velocity) and
climbs to it over its first 30 seconds, each next song climbs from the previous one's over its first 20.

## Install

[![PyPI](https://img.shields.io/pypi/v/zentris)](https://pypi.org/project/zentris/)

Packages for Linux, macOS and Windows are on [PyPI](https://pypi.org/project/zentris/). Run them with
[uv](https://docs.astral.sh/uv/) as above, or install them:

```sh
pipx install refall        # or: uv tool install refall
refall ~/Music     # with no argument it plays the default YouTube playlist
```

Quote YouTube URLs (`&` is special in the shell). YouTube also needs a JavaScript runtime
([Deno](https://deno.com) or Node.js) on your system.

The window carries the REFALL logo ([`data/icon.png`](data/icon.png)). On Linux desktops that take icons from desktop
entries (Wayland, the app grid), `tools/install-desktop.sh [path/to/refall]` installs a `refall.desktop` entry and the
icon for the current user, so the taskbar and Alt+Tab show it.

## Controls

Keyboard and gamepad both work at the same time. A gamepad is detected at startup and on hot-plug,
and the on-screen hints follow whichever device you used last.
Controllers are recognised through the bundled [SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB)
(`third_party/gamecontrollerdb.txt`; extra mappings can be given in `SDL_GAMECONTROLLERCONFIG`). A controller
missing from it still works with a generic layout.

| Action | Keyboard | Gamepad |
|---|---|---|
| Move | ← → | D-pad / left stick |
| Soft / hard drop | ↓ / Space | Down / Up |
| Rotate | ↑ or X (Z or J to rotate left) | A (B/X to rotate left) |
| Hold | C / Shift (tap) | LB / RB |
| Slow motion (when the gauge is full) | V / E | ZL / ZR (LT / RT) |
| Next universe (with `--universe`: its next variant, a new seed) | T | Y |
| Next song | N / Tab / Enter / PageDown | Back |
| Seek ±10 s in the song (testing) | Ctrl+Shift+Left/Right | |
| Next level (debugging) | L | |
| Fill the bonus gauge (testing) | B | |
| Pause | Esc / P (Q quits while paused, or goes back to the menu) | Start |
| Fullscreen | F / F11 | |

## Tools

- `refall-scope [songs...]`: see what the game hears (the song's structure, phases, beats and spectrum), and write
  notes on it.
- `refall-scene [UNIVERSE[:VARIANT]]`: browse universes, variants, song phases and transitions while the computer plays.

Both come with the package (`uvx --from refall refall-scope`). Every option, the tools and how the music drives the
game are in [docs/reference.md](docs/reference.md).

## Build from source

```sh
sudo apt install cmake g++ libglfw3-dev libglew-dev    # Debian/Ubuntu
cmake -S . -B build && cmake --build build -j
./build/refall ~/Music
```

`-DZENTRIS_DISABLED_UNIVERSES="pixel;cloudscape;synthwave;garden;magnets;sunprint;embroidery;sushi;ripples;starchart;stainedglass;groceries;musicbox"` (the default) lists the universes left out of the build;
`-DZENTRIS_DISABLED_UNIVERSES=""` builds them all.
