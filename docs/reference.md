# Reference

Everything beyond the [README](../README.md): command-line options, tools, how the game reads the music.

## Menu and modes

Started with no songs, the game opens on its menu, a crystal kaleidoscope (the game is called **Refall** for
now: `GAME_TITLE` in `src/menu.hpp`, shown on the title screen and as the window title): **VOYAGE**, **FREE PLAY**
or QUIT (arrows or D-pad, Enter / A to choose, Esc / B to go back). On the first launch (or after `--reset`) the title opens with a short, sober
animation. The game's own universes are folded into the
kaleidoscope: on the title and in free play they turn in turn, on Voyage it shows the most colourful of the focused stage's
universes (the focused song's in an open petal), graded toward the stage's hue: a saturation boost, a contrast stretch
from the snapshot's own luminance range and a duotone between a deep and a bright shade of the stage colour (greyed and
dimmed for a locked stage); free play takes the
focused playlist's hue. The world is then cut into gem: read through a ramp in that hue (from a near black of it to a pale light, drawn toward the world's own colours, each shard leaning to a neighbouring hue: magenta, amber, green, teal, violet... as the universes and stages change), small
facets and larger shards each lit from its own side and meeting edge to edge (no lead lines), a prism's rainbow on some, glints
twinkling and a soft light flare in the top right corner. The rosettes (the title's, Voyage's stages) are kept quiet:
a soft shade and one fine ring round the petals, a
polished dark disc in a gold ring at the middle; petals are faceted gems (dark ones with padlocks while locked), and
the panels are frosted glass over the kaleidoscope. They come from snapshots of each universe cached in `~/.cache/refall/menu-worlds/` (640x360);
the missing ones are made one at a time on a worker thread with its own OpenGL context, only while no key has been
pressed for a moment (the first launch fills the cache in about a minute on the title), and until a snapshot is ready
its place is procedural glass in the universe's colours, cross-fading to the image when it arrives. The menu remembers
where it was (`menu-state.tsv` next to the progress: the title entry, Voyage's difficulty, the voyage open and its focused stage, free
play's difficulty and playlist) and opens there on the next launch (`--reset` forgets it). The menu has its own music,
[`data/menu_music.mp3`](data/menu_music.mp3), looping seamlessly (a 1 s crossfade at its loop point) at about 70% of a
game song's level: it fades in when the menu appears, fades out (0.8 s) when a game starts and starts over from the top
back on the menu (`--no-menu-music` keeps the menu silent; `--mute` silences everything; offline renders never play it).
The file is found in the sources' `data/`, else next to the executable (installed builds ship it there). The menu plays
no song previews.

- **Voyage** first shows the voyages as free play shows its playlists: glass cards of one size, two per row (the grid
  scrolls to the focused row, three rows in view), the game's own first (**Refall**, featured: a gold rim), then the ones made from the player's playlists (the last played first), then
  **+ ADD A PLAYLIST**: paste a YouTube playlist link or a bare playlist ID (`PL...`), Enter reads it, then its name
  (the playlist's title to start with, editable), Enter registers the voyage and opens it. Each card has a small
  rosette, a kite per stage, the cleared ones lit, and how far the voyage went (its songs beaten on any difficulty,
  in %); Delete twice deletes a player's voyage with its progress. A voyage
  made from a playlist takes up to 60 of its songs (60 drawn at random, in the playlist's order, from a longer one) in
  stages of 4, each in a universe of the main campaign dealt at random (each once before any comes back, never twice in
  a stage) with a random variant, at the velocity the main campaign has at the same point of its journey: the same curve
  for 60 songs, its 15 stages compressed onto fewer for a shorter playlist. It plays like the main one (stages cleared
  in a row, unlocks, scores, the debriefs) with its own progress, in `voyages/` next to the campaign progress:
  `voyages.tsv` (the index), and per voyage `<id>.tsv` (`data/campaign.tsv`'s format, editable) and
  `<id>-progress.tsv` (the code: `src/voyages.cpp`).
- **Free play** is the game as it always was: the playlist in a random order, a universe picked for each song, levels
  rising with the lines (one per 20 lines, up to level 40: the Voyage's velocities, on the same scale). Each song
  starts with a warm-up: half the level's velocity, back to all of it over its first 20 seconds (EXPERT: from 80%, over 8 s). Choose a difficulty:
  EASY, NORMAL, HARD or EXPERT, a multiplier on the gravity of every level (x0.6, x0.8, x1, x1.4: HARD is the full
  speed, and goes 20G as the Voyage does: the drops from level 30 on, the choruses too at level 40; EXPERT falls 1.4
  times as fast, goes 20G from level 24 on and has a shorter lock delay, see below), a **starting level** (the
  strip at the bottom of the screen: 1, then every 5 up to 40; `--level N` on the command line, any level from 1 to 40), and a **playlist**: down to the PLAYLIST row of glass cards, the game's
  own first (featured: larger, a gold rim), then the ones used before (most recent first), each with its own hue and
  small rosette drawn from its playlist ID; **+ ADD A PLAYLIST** opens a field where a YouTube playlist link, a bare
  playlist ID (`PL...`) or a single video link is typed or pasted (Ctrl+V); Enter reads it (its song count, and its
  title through yt-dlp) and keeps it. Delete forgets the selected one. They are remembered in `playlists.tsv` in the
  user data folder (url, title, songs, last use; `--playlists FILE` for another file). A game over holds until a key
  is pressed, then goes back to the menu.
- **Voyage** (the campaign) is 15 stages of 4 songs played in a row. There are no levels: each song has its velocity
  (a game level, fixed for the whole song) and its universe set by the voyage, higher with each stage and song, and
  with the difficulty (EASY, NORMAL, HARD, EXPERT, chosen at the top of the Voyage screen). The stages are 15 glass petals in
  a rosette, numbered 1 to 15, each in its own hue, a gold arc with arrows showing the order and the next stage to clear glowing. Left /
  right moves between the petals (the rosette turns the focused one to the right, where its songs are listed), up
  reaches the difficulty. **Enter plays the whole stage** from its first song; **Space** (or Tab, gamepad X / Y) opens
  the petal, fanning out its songs: left / right chooses one, Enter plays the stage from that song to its last
  one, Esc or Space closes the petal. A locked petal can be focused, not played. Surviving all the songs of a run
  started from the first one **clears** the stage and unlocks the next one (on that difficulty and every easier one:
  a stage reached on HARD is playable on NORMAL and EASY too, not done there). On EXPERT a stage also opens once it
  is cleared (or its songs all beaten) on HARD. A run started from a later song is fine for practice: the songs it survives count as beaten (with their
  best score) but the stage isn't cleared; once all of a stage's songs are beaten, in any runs, the next stage opens
  too (quietly: only a clear turns the petal and celebrates). A game over ends the run; a key goes back to the stages. The game remembers,
  per difficulty, the stages cleared (with the run's score and lines) and the songs beaten (with the points scored and
  lines cleared during each, and the most lines ever cleared during it), shown in the Voyage's song list, stage header
  and info panel (`-` for lines recorded before they were kept), in `campaign-progress.tsv` in the user data folder
  (`~/.local/share/refall/` on Linux). The file keys songs by their video id and stage clears by their songs, so
  regrouping the Voyage keeps the songs beaten: a clear of a stage that no longer exists is dropped, and the stages it
  had opened (and every stage with a beaten song) stay open. A file from the first Voyage (12 stages of 5, keyed by
  index) is migrated the same way on load, the old file kept as `campaign-progress.tsv.bak`. Records name their
  difficulty `EASY`, `NORMAL`, `HARD` or `EXPERT2`: EXPERT is written `EXPERT2` because a bare `EXPERT` is HARD's
  former name, which files written before it still use and which is read as HARD.

A game over muffles the song like the slow-motion bonus does (a low-pass filter) and lowers its volume, then fades it out
to silence; the game over holds until the player presses a key (with songs given on the command line, a key plays
again).

| Option | |
|---|---|
| `--free` | free play straight away, without the menu |
| `--difficulty easy\|normal\|hard\|expert` | free play's difficulty (default: hard; or `e`, `n`, `h`, `x`), or a gravity multiplier such as `0.7`; also Voyage's |
| `--stage S[:SONG]` | a Voyage run straight away (stage S, from its song SONG) |
| `--unlock` | unlock every Voyage stage (saved) |
| `--reset` | forget the Voyage progress (prints what was forgotten) |

Songs or playlists given on the command line play free play straight away, as before; so do the offline renders
(`--review-shot`, `--clip`, `--analysis`, ...).

Without a display, `--menu-shot OUT.png [--menu-screen title|campaign|free] [--menu-hover S:SONG] [--menu-petal]`
saves a menu screen (`--menu-petal`: the hovered stage's petal open; `REFALL_TEST_KEYS` presses keys before it) and
`--menu-clip OUT.mp4` records a 34 s walk through the menu (needs ffmpeg; `REFALL_MENU_CLIP_SECONDS` changes its
length), printing the menu's average CPU and GPU time per frame; both can show made-up progress and playlists
(`--menu-demo`, always on in clips, never saved; `--menu-demo normal-done`: every stage cleared on NORMAL, the first
five on EASY, none on HARD). `--menu-screen debrief-over` or `debrief-clear` shows the debrief that ends a Voyage
run (a game over, a stage cleared: the logo, the score and lines) for a made-up run, `--menu-time S` taking the shot S
seconds into it (with `--menu-clip`, the clip is its animation, no key pressed). Its logo is the title's
ringed planet, drawn live (in silver after a game over). The menu's font, Josefin Sans, is OFL, in `src/embed/`
with its licence.

### The Voyage file

The campaign is [`data/campaign.tsv`](data/campaign.tsv), one song per line, tab-separated (`#` starts a comment):

```
# stage  song  id           universe             easy  normal  hard  expert  title
1        1     H_9xqc084UA  PRISM:AURORA         3.0   5.0     9.0   16.0    Small Town Kid - Shards
15       4     9Sh9ZSecyFU  SKY WHALE:MOONLIGHT  11.0  22.5    35.5  40.0    callah x Laura Brehm - skyline
```

- `stage`, `song`: the stage (1..) and the song's place in it (1..); any number of stages and songs works.
- `id`: the YouTube video id (songs are downloaded and cached like the playlist's).
- `universe`: `NAME` or `NAME:VARIANT` as `--universe` takes it (empty: picked for the song as in free play).
- `easy`, `normal`, `hard`, `expert`: the song's velocity on each difficulty, as a game level from 1 (the first level)
  to 40 (20 is ten times velocity 1's gravity, 40 about twenty times); decimals are fine. `expert` may be left out
  (a line of seven columns and the title, as files written before EXPERT): it is then HARD's + 7, at most 40, which
  is also how the shipped Voyage sets it.
- `title`: shown in the menu and the title card.

The game reads the file from the sources when it starts (or `--campaign FILE`), else from next to the executable,
else the copy built into it. The shipped Voyage tours 60 universes once each (SKY WHALE in its MOONLIGHT variant
ends stage 15) on songs of the default playlist. Each stage builds to a climax: its last song is faster than its first
(HARD +5.1, NORMAL +4, EASY +2.5), over a base that rises across the stages: HARD goes from 9 to 14.1 on stage 1 and
from 30.4 to 35.5 on the finale, NORMAL from 5 to 9 and from 18.5 to 22.5, EASY from 3 to 5.5 and from 8.5 to 11.
On HARD, from velocity 30 on, the drops go into hyperdrop (20G: a piece falls to the stack the moment it appears, with
a longer lock delay to slide and turn it along the stack; at velocity 40, free play's top, the choruses too), the rest
of the song keeping a visible fall: in the last song of stage 13, the last two of stage 14 and every song of the finale.
On EXPERT (HARD + 7: 16 to 21.1 on stage 1, 37.4 to 40 on the finale) hyperdrop comes from velocity 24 on, under the
same rules (drops, and choruses too at 40), from the last song of stage 6 on; its fall is also 1.15 times as fast as its
velocity says (beyond 40 on the finale, under the 60 rows/s cap). The lock delay is 0.55 s down to 0.5 s, and 0.5 s in
hyperdrop (a fast fall never leaves less time to slide a piece than hyperdrop); on EXPERT it is 0.4 s throughout. Up to
15 slides or turns of a resting piece restart it (10 on EXPERT). The
lock delay's clock starts once the piece is seen to rest (the drawn piece trails the real one a little in a fast fall).
A song's velocity is not a constant: the run's first song starts as a chill warm-up (a third of its velocity) and
climbs to it over its first 30 seconds, each next song climbs from the previous one's over its first 20. EXPERT's
warm-ups are short: the first song from 70% over 10 s, the next ones over 8 s. Over a song's first 20 s (EXPERT: 8)
the fall's pace stays at most that of a build, even if the song opens on a chorus or a drop.


## Controls in detail

Keyboard and gamepad both work at the same time. A gamepad is detected at startup and on hot-plug,
and the controls page in the pause menu follows whichever device you used last.
Controllers are recognised through the bundled [SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB)
(`third_party/gamecontrollerdb.txt`; extra mappings can be given in `SDL_GAMECONTROLLERCONFIG`). A controller
missing from it still works with a generic layout.

Testing and debugging keys:

| Action | Keyboard | Gamepad |
|---|---|---|
| Seek ±10 s in the song (testing; in the Voyage only with `--cheat-skip`) | Ctrl+Shift+Left/Right | |
| Next level (debugging) | L | |
| Fill the bonus gauge (testing) | B | |
| Complete the song; on the Voyage's screen, clear the focused stage (testing, with `--cheat-skip`; S is then no soft drop) | S | |

### Playing from a phone

`python3 tools/stream.py` (from the sources, after a build; needs `aiohttp` and `Pillow`) runs the game here, without a
window, and streams it to a phone's browser on the same Wi-Fi: open the address it prints and accept its self-signed
certificate (the browser's Gamepad API needs HTTPS). A controller paired with the phone plays as a gamepad; without
one, the page shows touch buttons (the gamepad's D-pad, A, B, LB, Back and Start). The frames go as JPEG at 60 fps
(about 3 MB/s) at the phone's aspect ratio, the sound with them. Arguments after `--` go to the game.


The window carries the REFALL logo ([`data/icon.png`](data/icon.png)). On Linux desktops that take icons from desktop
entries (Wayland, the app grid), `tools/install-desktop.sh [path/to/refall]` installs a `refall.desktop` entry and the
icon for the current user, so the taskbar and Alt+Tab show it.

## Command-line options

YouTube playlists (or single videos) work too, through [yt-dlp](https://github.com/yt-dlp/yt-dlp)
(install it with `pipx install "yt-dlp[default]"`; YouTube also needs a JavaScript runtime such as Deno or
Node.js, and Node.js is picked up automatically). Songs are downloaded when queued and cached as MP3 in
`~/.cache/refall/youtube/`. Downloading from YouTube is against its terms of service: personal use only.

```sh
./build/refall "https://www.youtube.com/playlist?list=PLWHPu2N_Gb2lbZ8-7sYKSEbDlb3UiAVye"   # quote it: & is special in the shell
./build/refall-scope "https://www.youtube.com/watch?v=..."
```

Options: `--in-order` (play songs in order: folders alphabetically, playlists in their order; the default is a random order), `--seed N` (repeat a run), `--universe NAME[:VARIANT]` (play every song in that universe, e.g. `--universe vector`
or `--universe "vector:p3 amber"`, as the bottom-left corner names it; the terminal prints the command that shows
the same universe again), `--universe-on-peak` (a long song may also change universe when it rises into a peak),
`--autoplay`, `--mute`, `--size WxH`,
`--shots PREFIX N` (renders N screenshots of different universes and exits), `--phase-shots PREFIX` (one screenshot per scene level of a song),
`--transition-shots PREFIX N` (at `--shot-time` in the song, with a filled board, changes from `--universe` to `--to NAME`
with `--transition NAME`, saves N frames across the change and exits),
`--free`, `--difficulty`, `--stage`, `--unlock`, `--reset`, `--campaign FILE`, `--progress FILE`, `--playlists FILE`,
`--no-menu-music`, `--menu-shot`, `--menu-screen`, `--menu-hover`, `--menu-time`, `--menu-petal`, `--menu-demo` and `--menu-clip` (the kaleidoscope menu, Voyage and
modes: see [the README](../README.md#modes)),
`--clip OUT.mp4` (records a video with sound, via ffmpeg, from 4 s before the first drop of the song to 6 s after it,
with the game played at a calm pace; `--clip-before` / `--clip-after SEC` change the span).

`tools/scenereview.py` reviews universes: `generate` renders 100 random snapshots (each for a downloaded song, at a
calm, mid or peak part of it, with a filled board; `refall --review-shot OUT.png` makes one), and `serve [--lan]`
opens a dashboard, on a computer or a phone, to skip scenes or comment on them. Comments become threads that
`pending` / `watch` hand to whoever processes them, and `reply` answers one with before/after snapshots to accept
or discuss further.

`tools/promo.py [OUT.mp4]` makes a promo video: 5 random songs of the default playlist, each recorded around its
first drop in its universe, stitched with crossfades (`--songs N`, `--before`, `--after`, `--fade`, `--size`).

`tools/demo.py [tools/demo.json] -o demo.mp4` makes the demo video: the whole song of `tools/demo.json`, in chapters
(shots of the game in many universes, cut on the song's bars, none shorter than `min_shot`; optional title cards on
black, crossfades on the cuts), each showing a line clear (`--plan-only` prints the timeline, `-j N` records shots in
parallel, cached between runs). It uses `refall --analysis OUT.json` (the song's beats, structure and scene
levels as JSON), the test hook `REFALL_CLIP_FILL=1` (a clip's warm-up starts on a filled board) and the
`[clip] clear N at T` lines a clip prints.

Test hook for the menus and Voyage: `REFALL_TEST_PLAY=1` runs the game as when played (title screen, breaks, game
overs) without a window, the songs on a simulated clock, with `REFALL_TEST_KEYS="T:KEY,..."` (up, down, left, right, ok,
back, alt (Space), pause, quit, skip (S with `--cheat-skip`) pressed at T seconds), `REFALL_TEST_SHOTS="T:FILE.png,..."`, `REFALL_TEST_QUIT=T` and `REFALL_TEST_TAIL=S`
(each song starts S seconds before its end), e.g. a stage run through with `--autoplay`. Measuring the menu:
`--menu-clip` prints its CPU and GPU time per frame; `REFALL_MENU_FINISH=1` adds the wall time with `glFinish` (software
renderers, whose timer queries mean nothing), `REFALL_MENU_KALEIDO_DIV=N` draws the kaleidoscope at 1/N of the resolution (it is full resolution by default),
`REFALL_MENU_SLOW=1` lists the frames over 30 ms; the clip also prints the worst frame within 0.6 s of a key.

## refall-scope: see what the game hears

`./build/refall-scope [songs or folders...]` (the default playlist when no song is given) plays a song and shows the analysis that drives the game:
- a phase timebar with the scene levels (calm / mid / peak) and where the scene changes happen
- the detected structure: intro, verse, build, chorus, drop, break, outro, with similar parts grouped
- pulse zones, a 16-band spectrum, loudness, intensity and onsets, and bar lines
- a live readout of the current section and the game's density/speed/glow profile, with the beat in the bar
- a zoomed detail view with the beat grid
- four beat-aligned samples cut from the song (an experiment for line clears, not used by the game yet):
  HIT (one clean transient, 1 beat), LOW (a bass-led half bar), TONE (a sustained, steady half bar) and
  HOOK (the first bar of the most repeated chorus/drop)

Space plays/pauses, Left/Right seek 5 s, Up/Down zoom the detail view, N/P change song, click to seek,
1–4 or a click on a sample button plays that sample (over the song, or alone when paused).
The SPEED buttons (top right) slow the song down to ×0.9 or ×0.5, tape-style (the pitch drops too);
click the active one again to go back to normal speed.

**Notes:** press A to write a note at the playhead (e.g. "the drop should start earlier here"); Enter saves it,
Esc cancels (the song pauses while you type). Notes go to `annotations.jsonl` in the user data folder
(`~/.local/share/refall/` on Linux; `--notes FILE` to change it), one JSON object per line with the song, the time,
the text and what the analysis said there (the phase and section, and the whole scene plan and structure).
They show as cyan flags on the timeline, with their text in the detail view.
`--song ID|TITLE` starts at a song of the playlist (a YouTube id or part of its title), `--at SEC` at a time, and
`ytdl:<video id>` plays a single YouTube song, and `--shuffle` plays the songs in a random order. Playlist listings are cached (and refreshed in the background for the
next run) and songs stay downloaded in the cache, so reopening an annotated song is instant and works offline.

The game and refall-scope share the same analysis and song plan code (`src/songplan.*`), so what you see is what the game uses.

## refall-voyage-viewer: review a campaign

`./build/refall-voyage-viewer [campaign]` plays a campaign's songs in a row, each in its universe and variant at its
velocity, the computer playing (a game over starts a new board at once, the song plays on). The campaign is the
main one by default (`data/campaign.tsv`, as the game reads it), or a campaign file, or a custom voyage (its id or
its title). An overlay (Tab or H hides it) shows:
- the campaign's songs by stage, each card the universe in miniature (the menu's snapshots), the video's thumbnail
  (downloaded once into `~/.cache/refall/thumbs/`), its title, universe and velocity; the song playing is lit, and
  a song being downloaded or prepared says so
- the song's timeline: its sections (intro, verse, build, chorus, drop, break, outro), the scene levels over them
  (calm, mid, peak) and the playhead

The tour (on at the start; Space) plays each section about 6 s, from 1.5 s into it, then goes on to the next song.
Taking over pauses it: Left/Right seek 10 s, `[` / `]` go to the previous / next section, Up/Down to the previous /
next song, Page Up/Down to the previous / next stage; a click on a card plays its song, a click (or a drag) on the
timeline seeks. Songs not downloaded yet are fetched when played, and the next one is prepared in the background.

Options: `--difficulty easy|normal|hard|expert` (the velocities played, NORMAL by default), `--stage N --song K` (start
there), `--size WxH`, `--fullscreen`, `--mute`, and `--shot OUT.png` (saves the viewer `--shot-after SEC`, 8 by
default, into the first song, offscreen, then exits; `REFALL_VIEWER_KEYS="T:COMMAND,..."` gives it commands on the way:
next, prev, stage+, stage-, fwd, back, sec+, sec-, tour, hide).

## Slow motion and quad rewards

Each cleared line charges one step of the bonus gauge, a ring below the next pieces (12 steps to fill it, 20 on
EXPERT; a quad charges 5). Once it's full, the slow-motion button starts the bonus: for 10 s (5 s on EXPERT) the gauge drains, the
pieces don't fall at all (whatever the velocity, 20G included), and a piece resting on the stack doesn't lock by itself: only a
hard drop sets it down, so starting the bonus as a piece is about to lock gives all of it to think, or to hold; when it ends, a
resting piece gets a fresh lock delay. The song keeps playing normally: only a very
slight tape-like slow-down (and back) marks the start and the end. Use it to clean up the stack in a part that's too fast. Gravity gets
back on the beat when the bonus ends. Meanwhile the universe shows the slow motion its own way (VECTOR sweeps in
the complementary phosphor with long trails), under a look common to every universe: outside the well the world
drains to a cold silver, darker, with a vignette and a pale halo round the well; the well and the gauge keep their
colors, and a bright ring round the gauge empties with the time left.

A quad scores a 400-point bonus (800 back-to-back) and the universe celebrates it (VECTOR: a vector shockwave
and beam overdrive).

## How the music shapes the game

Each song is decoded and analyzed in the background (under 1 s):

- **Song sections**: gravity counts beats of a fixed 130 BPM (not the song's tempo: the same velocity falls as fast in every song), at a pace set by the current section, in rows per beat: break ×0.4, intro ×0.5, outro ×0.5, verse ×0.8, build ×1, chorus ×1.2, drop ×1.3 (at most ×1 in a song's first 20 s). During a change of universe gravity eases down to ×0.25 (the stack can be hard to read while two worlds blend) and back up over about a second once the new one is in.
- **Levels**: one level per 20 lines, up to level 50 (980 lines). Each level speeds up the beat-locked gravity (the levels spread over the velocity 1 .. 40: ×10 at velocity 20, ×19.5 at 40, capped at 60 rows/s) and, up to level 20, shortens the lock delay (0.55 s → 0.5 s; EXPERT: 0.4 s throughout). Every song starts with a warm-up (half the level's velocity, all of it after 20 s; EXPERT: 80%, after 8 s). On HARD, from level 38 on (velocity 30), the drops go 20G (at level 50 the choruses too), as in the Voyage; on EXPERT from velocity 24 on. Game over resets the level.
- **Key**: the song's hue follows the circle of fifths; universes may pick their variant from it (VECTOR's phosphor).
- **Tempo, brightness, bass/air balance, dynamics, density**: weigh which universe suits the song.
- **Song structure**: the song is split at bar lines into labelled segments (intro, verse, build, chorus, drop, break, outro), and segments that sound alike are grouped. A section that gets louder starts where it lands, on the first beat where its bass and loudness are back (after the gap, bass cut or riser before a drop), and a drop is the song's most intense material entered by a rise. That rise is measured against the section before it, or, when the section starts on a bass return, against how the one before it ended (a loud section that thins out into a breakdown keeps a high mean: the drop after it is no louder than it on average, only than its end). A drop is heard where the full bass comes back after a cut, so a bass return (a beat whose bass reaches the next two bars' level after a bar or more of clearly lower bass, into loud material) places a section boundary on that beat; a cut of a bar or so inside a section that already had the full bass is a fill and is ignored. A drop can also come out of a loud chorus without getting louder on average: after a gap of a beat or two, its hits land clearly harder (bass and loudness peaks above those of the steady full-bass bars before it, as in a pumping, sidechained drop); that landing starts a drop too. The gap may be shallower (a beat or two at 60% rather than cut) between two stretches of steady full bass: a pre-drop that already has the bass, then the drop itself. When two loud sections follow each other and the second is clearly fuller, the first is its build and the second the drop; a section's energy leaves out the fade at its tail (the song ending), so a final drop still counts. Each segment eases the scene's density, speed, glow and saturation (builds ramp up, breaks thin out).
- **One universe per song**: at most three intensity levels (calm, mid, peak) are staged by the universe itself (VECTOR: a lone oscilloscope trace, a scrolling vector grid, spectrum mountains). Rising changes are quick and anticipated so they land on the boundary, falling ones slow and soft; a new section inside the same level gets a light refresh.
- **Calm by design**: visuals follow slow (~1 s) envelopes of the music and there is no camera shake or flashing. Beat pulses appear only during peak sections (choruses, drops), stronger for faster songs; songs above ~110 BPM also get soft hits on strong transients there.
- **Live bands and loudness**, heavily smoothed, drive particle motion and glow.

## Releasing

`.github/workflows/wheels.yml` builds wheels for Linux (x86_64, aarch64), macOS (x86_64, arm64) and
Windows (x86_64) plus a source distribution on every push, and publishes them to PyPI when a `v*` tag is
pushed (`git tag v0.1.0 && git push --tags`). Publishing uses PyPI trusted publishing: on pypi.org, add a
publisher for project `refall`, repository `Gregwar/refall`, workflow `wheels.yml`, environment `pypi`.
