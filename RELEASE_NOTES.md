Free polyphonic synth plug-in for macOS (Apple Silicon), VST3, Audio Unit and CLAP. By Dani Koenig.

### New in v2.1.1
- **Frame**: the window now has the same 4 px orange frame as Gain, Dub Delay and Remember, so all plugins share one look

### New in v2.1.0
- 43 factory presets in nine categories: Pad, Bass, Lead, Keys, Brass, Pluck, Chord, Seq, FX (own sounds), shown as submenus in the preset menu
- Chord presets with CHORD on, Seq presets with a running sequencer
- New effect type DUB DELAY in all three FX slots: the chain of the Dub Delay plug-in (ping-pong delay, filter in the feedback, saturation, band-pass with LFO, low cut)
- Factory presets matched in level
- Manuals updated

### New in v2.0.1
- Cleaner layout: all sections in the third row start at the same height, VOICE ALLOCATION and CHORD line up with EFFECT 2 and 3, oscillators end in line with EFFECT 1

### New in v2.0.0
- Chord section (blue): 18 ready chord types (Major to Power) plus LEARNED, every key plays the chord transposed
- INVERSION (Root, 1st, 2nd, 3rd) and VOICING (Closed, Open, Drop 2, Wide)
- BASS: adds a bass note one or two octaves below the key
- SILENT now works on its own: learn a chord without sound
- After learning, the chord type switches to LEARNED and CHORD turns on
- Manuals updated

### New in v1.6.0
- CLIP: Knack clipper in OUTPUT before the volume (up to +24 dB, 4x oversampled), 0 % = clean
- CLIP and VOLUME as two matching knobs, PANIC smaller
- Start screen removed; the plugin opens with the preset Init. Factory presets without numbers
- Version number shown at the top of the display
- Manuals updated

### New in v1.5.0
- Presets: save, load and step through your own sounds from the bar at the top of the display (< / >, menu, SAVE, DEL)
- A preset stores everything: all controls, every sequencer step and the learned chord
- Categories as subfolders; files (.theone) in Music / Dani Koenig / The One / Presets, usable in every DAW
- Factory programs are part of the preset list (START PROGRAM in OUTPUT removed)
- Manuals updated

### New in v1.4.5
- Sequencer follows the DAW transport: with the DAW stopped it runs on its own, when the DAW plays it starts at step 1, locks to the DAW grid and stops when the DAW stops
- Keyboard shows the notes the synth plays: your key bright, chord memory and sequencer notes in a softer orange
- Manuals updated

### New in v1.4.4
- **Manuals**: new PDF manuals in English and German, attached to this release (also linked on the download page)
- Manual covers quick start, every control, recipes and install

**Download `TheOne-Installer.pkg`** and double-click it. macOS blocks it the first time: open System Settings > Privacy & Security and click "Open Anyway". Full guide: see INSTALL.txt in the ZIP or in the repository.

### New in v1.4.3
- No more clicks when voices are stolen: a busy voice fades out in 2 ms before it takes the new note (big unison pads)
- Changing the start program switches CHORD off (the learned chord stays stored)
- All GUI texts in English

### v1.4.2
- New bass program "04 - Bass - Sub": saw + square at 8', sine sub one octave below, punchy 24 dB ladder envelope, phase reset

### v1.4.1
- Clean output limiter (1.5 ms lookahead, ceiling -1 dBFS): big chords and pads no longer distort
- Init around -9 dBFS so chords have headroom; Pad and Bass programs slightly lower

### v1.4.0
- Sequencer random: RND switches for PITCH (borrows the pitch of another active step), OCTAVE (jumps up or down) and ACCENT (moves around), with a shared RND amount
- VEL: random velocity from fixed to loose
- Tidier sequencer controls: SEQ ON / LATCH, RATE / ORDER side by side, LEN, GATE, RND, VEL in one row

### v1.3.1
- Oscillator RESET renamed to PHASE RESET (each note starts the wave at its beginning), TRI/SAW moved next to the octave selector

### v1.3.0
- Louder Init (around -6 dBFS)
- Big blue VOLUME knob, TEMPO only in the Standalone app (in a DAW the host tempo is used)
- Clearer step sequencer: taller pitch faders with value readout, OCTAVE as a button per step (0 / +1 / -1), row labels, steps grouped in fours

### v1.2.2
- Effect sync with dotted and triplet values (1/1 to 1/32 T)
- LFO rate in sync mode shows note values (e.g. 1/8 D)

### v1.2.1
- BEND range knob moved next to the pitch and mod wheels
- Sequencer in orange

### v1.2.0
- Step sequencer: SLIDE and ACCENT buttons per step (replace pulse count and gate type), bigger pitch and octave faders
- Pitch bend range in whole semitones (+/- 0 to 24), shown as BEND RNG
- Darker, lower keyboard
- Display: chord line removed, bigger scope

### v1.1.1
- Display with a glowing blue frame
- Bigger level knobs in the mixer

### v1.1.0
- Step sequencer: OCTAVE row per step (-1 / 0 / +1), all steps start in the middle, wider layout
- More colour: each section has its own accent (oscillators orange, LFOs violet, filters cyan, envelopes green, effects blue, sequencer red)
- Cleaner layout: display framed like the other sections, all boxes on one grid, smaller OUTPUT

### v1.0.0
- 16 voices, 3 oscillators (wave angle, pulse, hard sync, FM, beat), sub oscillator (-1/-2 oct, sine or square), ring mod, noise (white / red / purple)
- SVF and Ladder filter, serial or parallel, 3 envelopes (DAHDSR), 4 LFOs with tempo sync
- Unison, glide, mono, HOLD
- Chord Memory: learn a chord, every key plays it rigidly transposed
- 3 effect slots: delays, chorus, flanger, phaser, bit crusher, resonator
- 16-step sequencer: pitch (semitones), pulse count, gate type (rest / one / multi / hold), step ON / MUTE / SKIP, order FWD / REV / PING / RANDOM, host sync
- Start screen with 5 starting points (Init, Pad, Lead, Bass, Pluck), can be switched off
- PANIC button, on-screen keyboard, resizable window

Requirements: Apple Silicon Mac (M1 or newer), macOS 11 or newer.
