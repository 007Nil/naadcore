# 📊 HARMONIUM COMPANION - CODEBASE ANALYSIS (PASS 1)

## 1. PROJECT STRUCTURE & MAJOR DIRECTORIES

```
harmonium-companion/
├── main.js              # Application entry point (694 lines)
├── index.html           # Main HTML structure
├── style.css            # All styling (1152 lines)
├── sw.js                # Service worker for PWA
├── manifest.json        # PWA manifest
├── harmonium.sf2        # SoundFont file (sample library)
├── images/              # Static assets
│   ├── harmonium.png    # Background instrument graphic
│   ├── background.png   # App background
│   └── *.png            # PWA icons
├── .github/
│   └── workflows/       # GitHub Actions deployment
└── LICENSE              # MIT license
```

**Architecture**: Single-page application (SPA) with no build step. All code is vanilla JavaScript with ES modules.

---

## 2. APPLICATION ENTRY POINTS

**Main entry**: `index.html:175` → loads `main.js` as ES module
- `index.html:51` Start button triggers audio context initialization
- `main.js:617` `onclick` handler for `#start-btn` initializes entire application

---

## 3. FRAMEWORKS & IMPORTANT DEPENDENCIES

**Primary dependency** (loaded from CDN via import map):
- **Spessasynth** (`spessasynth_lib@4.3.0`) - Web Audio SF2 synthesizer
  - Provides: SoundFont playback, MIDI synthesizer, AudioWorklet processor
  - Import: `https://unpkg.com/spessasynth_lib@4.3.0/dist/index.js`
  - Processor: `https://unpkg.com/spessasynth_lib@4.3.0/dist/spessasynth_processor.min.js`

**Native Web APIs**:
- Web MIDI API (`navigator.requestMIDIAccess`)
- Web Audio API (AudioContext, AudioWorklet, AnalyserNode)
- Service Worker API

---

## 4. AUDIO-RELATED FILES/MODULES

**File**: `main.js:1-694`

**Key audio components**:
- **WorkletSynthesizer** (line 629): Main synthesizer instance from Spessasynth
- **AudioContext** (line 618): Browser audio context
- **AnalyserNode** (line 637): Visualizer data extraction (fftSize: 1024)
- **GainNode** (line 633): Master volume control
- **SoundBank**: Loads `harmonium.sf2` (line 628-631)

**Audio pipeline** (line 633-644):
```
synth → volumeNode → destination
synth → analyser → volumeNode → destination
```

---

## 5. MIDI-RELATED FILES/MODULES

**File**: `main.js:251-298`

**Implementation**:
- **Setup**: `setupMIDI()` (line 251) requests MIDI access
- **Message handler**: `handleMidiMessage()` (line 286)
  - Parses MIDI status bytes (line 287)
  - Note On: `0x90` with velocity > 0 (line 288-294)
  - Note Off: `0x80` or Note On with velocity = 0 (line 289-296)
  - Maps MIDI notes 36-83 to harmonium keyboard (line 291)

**MIDI CC support** (implicit via Spessasynth):
- Volume: CC #7 (line 211)
- Expression: CC #11 (line 167, 569)
- Sustain: CC #64 (line 548)
- Decay: CC #72 (line 223)
- Reverb: CC #91 (line 229)
- Chorus: CC #93 (line 236)

---

## 6. SOUNDFONT/SF2/SAMPLE-RELATED FILES

**File**: `harmonium.sf2` (root directory)

**Loading** (main.js:628-631):
```javascript
const sfont = await (await fetch("./harmonium.sf2")).arrayBuffer();
synth.soundBankManager.addSoundBank(sfont, "main");
```

**Program change**: `synth.programChange(0, 0)` (line 647)

**Note**: No custom sample handling - entirely reliant on Spessasynth's SF2 implementation.

---

## 7. KEYBOARD/NOTE HANDLING

**File**: `main.js:57-156`, `index.html:134-136`

**Keyboard structure** (line 67-101):
- 48 keys generated dynamically
- Data indices: 0-47
- MIDI mapping: note = index + 36 (line 344)
- Visual keyboard: HTML divs with `.key` class
- Key types: `.white` and `.black` (line 72)

**Input handling**:
- **Mouse**: `onmousedown` (line 77-90), `onmouseup` (line 92-96), `onmouseleave` (line 98-100)
- **Keyboard**: `window.onkeydown` (line 670-683), `window.onkeyup` (line 685-689)
- **MIDI**: `handleMidiMessage()` (line 286)

**Velocity calculation** (line 80):
```javascript
const velocity = ((e.clientY - rect.top) / rect.height) * 0.7 + 0.3;
```

---

## 8. HARMONIUM-SPECIFIC LOGIC

### Bellows System (Air Reservoir)
**File**: `main.js:558-579` (`loop()` function)

**Parameters** (line 5-14):
- `reservoir`: Air volume (0-100)
- `isManual`: Manual/auto bellows toggle
- `pumpCharge`: Temporary air addition

**Physics** (line 560-564):
- Manual mode: Air drains based on active notes (drain = 0.02 + notes × 0.03)
- Auto mode: Reservoir stays at 100
- Expression controller: `reservoir / 100 * 127` (line 567-569)

### Octave Coupler & Sub-Octave
**File**: `main.js:350-356`, `507-415`

**Implementation**:
- **Coupler**: Adds note +12 semitones (line 351, 399-404)
- **Sub-Octave**: Adds note -12 semitones (line 355, 406-412)
- Toggle controls: `#coupler-toggle` (line 191-194), `#sub-oct-toggle` (line 186-189)
- Real-time switching: `refreshAudio()` (line 387-415)

### Raga System
**File**: `main.js:32-543`

**Data** (line 32-48): 13 ragas defined as note collections (Sargam-compatible)
- Example: Bilawal = [0,2,4,5,7,9,11] (major scale)
- Example: Kalyan = [0,2,4,6,7,9,11] (major with sharp 4th)

**Filtering** (line 303-313, 519-543):
- `isStrictRaga`: Permits only raga notes
- `applyRagaFilter()`: Highlights allowed notes with colors
- Transpose support: Adjusts filtering by `transposeShift`

### Sargam (Indian Notation) System
**File**: `main.js:454-505` (`toggleNotation()`)

**Mappings** (line 24):
- Indian: ["Sa","re","Re","ga","Ga","Ma","ma","Pa","dha","Dha","ni","Ni"]
- Western: ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]

**Octave dots** (line 483-490):
- Below: 1 dot (U+0308) or 2 dots
- Above: 1 dot or 2 dots
- Special handling for lowercase sargam (underlined)

---

## 9. UI-RELATED CODE

**File**: `style.css:1-1152`, `index.html:1-178`

**Layout structure** (index.html:57-166):
- `.zoom-viewport`: Scales entire app 1.45×
- `.synth-bg`: Harmonium background image
- `.brand-container`: Logo overlay
- `.raga-display-overlay`: Raga selector
- `.bellows-row`: Auto/manual toggle + meter
- `.top-ui-row`: Notation toggle, octaves, transpose
- `.controls-grid`: Volume, sustain, chorus, reverb sliders
- `.keyboard-wrapper`: Scrollable keyboard container
- `.master-panel`: Sub-oct, coupler, hold, MIDI controls

**Visualizer** (index.html:111, style.css:774-782):
- Canvas overlay on top of app
- Real-time waveform display (main.js:583-615)
- Smoothed data with 0.25 smoothing factor

**Styling** (style.css):
- CSS variables for gold/wood color scheme (line 1-10)
- Custom range inputs with gradient thumbs
- LED indicators (`.oct-led`)
- Active note highlighting (`.key.active`)
- Raga color coding: yellow/pink/blue/orange/green

---

## 10. EXTERNAL LIBRARIES

**Spessasynth** (primary):
- **Repo**: https://github.com/spessasus/spessasynth_core
- **Role**: SF2 playback, Web Audio synthesis
- **Features used**: 
  - WorkletSynthesizer class
  - SoundBank manager
  - MIDI note on/off
  - CC controllers (expression, sustain, reverb, chorus)
  - Program changes

**No other libraries**: Pure CSS, HTML, and Web Audio API for everything else.

---

# 🔄 HIGH-LEVEL RUNTIME FLOW

```
┌─────────────────────────────────────────────────────────────────────────┐
│ USER ACTION: Click "PRESS TO PLAY" (index.html:51)                      │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ init() in main.js:617-690                                               │
│ 1. Create AudioContext (line 618)                                       │
│ 2. Load Spessasynth processor (line 620-626)                            │
│ 3. Fetch harmonium.sf2 (line 628)                                       │
│ 4. Instantiate WorkletSynthesizer (line 629)                            │
│ 5. Add SoundBank (line 630-631)                                         │
│ 6. Build audio graph: synth → volume → destination                      │
│                    synth → analyser → volume → destination              │
│ 7. Resume context (line 646)                                            │
│ 8. Set program 0 (line 647)                                             │
│ 9. Remove overlay (line 650)                                            │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ POST-INITIALIZATION (main.js:651-690)                                   │
│ - initKeyboard() → Generate 48 keys with mouse/keyboard handlers        │
│ - setupUIButtons() → Attach controls (bellows, coupler, raga, etc.)     │
│ - setupMIDI() → Request MIDI access, attach input handlers              │
│ - toggleNotation() → Set initial label format                           │
│ - loop() → Start bellows physics (manual mode air reservoir)            │
│ - drawVisualizer() → Start canvas waveform rendering                    │
│ - Key listeners → keyboard.onkeydown/onkeyup                            │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ INPUT EVENT FLOW                                                        │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│ KEYBOARD/MIDI INPUT                                                     │
│ ┌──────────────────────┐      ┌──────────────────────┐                  │
│ │ Keyboard (main.js:670│      │ MIDI (main.js:286)   │                  │
│ │ - KeyMap: a→12, w→13 │      │ - Parse MIDI message │                  │
│ │ - Translate to idx   │      │ - Note On/Off check  │                  │
│ │ - Call handleKeyPress│      │ - Call handleKeyPress│                  │
│ └──────────────────────┘      └──────────────────────┘                  │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ NOTE EVENT PROCESSING (main.js:300-339)                                │
│ handleKeyPress(index, velocity)                                         │
│ 1. Check raga filter (line 303-313) if isStrictRaga                    │
│ 2. Handle drone mode (line 315-322)                                    │
│ 3. Add to heldKeys Set (line 324)                                      │
│ 4. Call startAudio(index, velocity)                                    │
│                                                                         │
│ handleKeyRelease(index)                                                 │
│ 1. Remove from heldKeys (line 330)                                     │
│ 2. If sustain: add to sustainQueue (line 334-335)                      │
│ 3. Else: call stopAudio(index) (line 338-339)                          │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ HARMONIUM LOGIC                                                         │
│                                                                         │
│ startAudio(index, vel) - main.js:341-367                              │
│ ┌───────────────────────────────────────────────────────────────────┐  │
│ │ 1. Calculate MIDI note: base=36+index, target=base+transpose     │  │
│ │ 2. Send noteOn to synth: synth.noteOn(0, target, vel×127)        │  │
│ │ 3. If coupler: noteOn(target+12)                                 │  │
│ │ 4. If sub-octave: noteOn(target-12)                              │  │
│ │ 5. Store in activeNotes Map with metadata                        │  │
│ │ 6. Highlight key UI element                                      │  │
│ └───────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│ stopAudio(index) - main.js:369-385                                    │
│ ┌───────────────────────────────────────────────────────────────────┐  │
│ │ 1. Retrieve note data from activeNotes                           │  │
│ │ 2. Send noteOff to synth (original note)                         │  │
│ │ 3. If coupler was active: noteOff(note+12)                       │  │
│ │ 4. If sub-octave was active: noteOff(note-12)                    │  │
│ │ 5. Remove from activeNotes Map                                   │  │
│ │ 6. Remove key highlight                                          │  │
│ └───────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│ bellows loop() - main.js:558-579                                      │
│ ┌───────────────────────────────────────────────────────────────────┐  │
│ │ Manual mode:                                                       │  │
│ │ - Air fills: reservoir += (target - reservoir) × 0.04            │  │
│ │ - Air drains: reservoir -= (0.02 + activeNotes.size × 0.03)      │  │
│ │ - Update CC #11 (expression): floor(reservoir/100 × 127)         │  │
│ │                                                                    │  │
│ │ Auto mode:                                                         │  │
│ │ - Expression fixed at 127 (CC #11)                               │  │
│ └───────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ SOUND GENERATION                                                        │
│                                                                         │
│ Spessasynth WorkletSynthesizer (external library)                       │
│ ┌───────────────────────────────────────────────────────────────────┐  │
│ │ 1. Receive noteOn(message) with channel, MIDI note, velocity    │  │
│ │ 2. Look up sample in harmonium.sf2 SoundBank                    │  │
│ │ 3. Play sample with ADSR envelope                               │  │
│ │ 4. Apply CC effects:                                              │  │
│ │    - CC #7 (Volume)                                             │  │
│ │    - CC #11 (Expression/Bellows)                                │  │
│ │    - CC #64 (Sustain)                                           │  │
│ │    - CC #72 (Decay/Release)                                     │  │
│ │    - CC #91 (Reverb)                                            │  │
│ │    - CC #93 (Chorus)                                            │  │
│ │ 5. Generate audio stream via AudioWorklet                       │  │
│ └───────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│ Audio output path:                                                      │
│ WorkletSynthesizer → GainNode (volume) → AudioContext.destination      │
│                   → AnalyserNode → GainNode → AudioContext.destination │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ AUDIO OUTPUT                                                            │
│ ┌───────────────────────────────────────────────────────────────────┐  │
│ │ - Browser audio system (speakers/headphones)                    │  │
│ │ - Visualizer canvas (real-time waveform)                        │  │
│ └───────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
```

---

# 🎯 FEATURE IMPLEMENTATION MAP

| Feature | Implementation Location | Status |
|---------|------------------------|--------|
| **MIDI Note On** | `main.js:288-294` (checks status & 0xf0 === 0x90, velocity > 0) | ✅ Implemented |
| **MIDI Note Off** | `main.js:289-296` (status & 0xf0 === 0x80 or velocity === 0) | ✅ Implemented |
| **MIDI velocity** | `main.js:290` (velocity / 127), `main.js:80` (mouse-based) | ✅ Implemented |
| **Polyphony** | `main.js:18-20` (activeNotes Map, heldKeys Set, sustainQueue Set) | ✅ Implemented via Spessasynth |
| **SoundFont loading** | `main.js:628-631` (fetch harmonium.sf2, addSoundBank) | ✅ Implemented |
| **Harmonium sound generation** | Spessasynth (SF2 playback with harmonium.sf2) | ✅ External |
| **Bellows/expression** | `main.js:558-579` (reservoir physics), `main.js:567-569` (CC #11) | ✅ Implemented |
| **Octave coupler** | `main.js:350-352, 375-377, 398-404` (note+12) | ✅ Implemented |
| **Sub-octave** | `main.js:354-356, 378-380, 406-412` (note-12) | ✅ Implemented |
| **Drone mode** | `main.js:315-322, 196-205` (sustain + hold logic) | ✅ Implemented |
| **Tuning/transpose** | `main.js:417-425` (transposeShift ±12), `main.js:345` (applied) | ✅ Implemented |
| **Reverb** | `main.js:226-230` (CC #91), Spessasynth effect | ✅ External |
| **Chorus** | `main.js:232-237` (CC #93), Spessasynth effect | ✅ External |

---

# 📁 FILES WORTH DEEPER ANALYSIS (PASS 2)

1. **`main.js:1-694`** - Complete application logic, bellows physics, MIDI handling
2. **`main.js:300-339`** - Note on/off logic with raga filtering
3. **`main.js:341-385`** - Polyphonic note management with coupler/sub-octave
4. **`main.js:558-579`** - Real-time bellows/reservoir simulation
5. **`main.js:454-505`** - Sargam notation rendering logic
6. **`style.css:1-1152`** - Complete styling system (zoom, UI layout, visual feedback)
7. **`index.html:1-178`** - UI structure and control layout
8. **`spessasynth_lib@4.3.0`** (external) - SF2 rendering engine (need to review source)
9. **`harmonium.sf2`** - Sound bank content (need to inspect with SF2 tool)

---

# 🔍 NOTES ON MISSING/UNCLEAR IMPLEMENTATIONS

- **No .mid file loading** - Mentioned in "in development" but not implemented
- **No MIDI CC visualization** - CCs are sent but not displayed
- **No PWA install prompt** - manifest.json exists but service worker is minimal
- **No mobile optimization active** - Media queries commented out in CSS
- **Unknown SF2 quality** - Cannot assess sample quality without external inspection

---

# 📊 PASS 2 — DEEP AUDIO, MIDI AND HARMONIUM ANALYSIS

## EXECUTIVE SUMMARY

This document provides deep technical analysis of the harmonium-companion implementation,
focusing on MIDI processing, polyphony, audio engine, bellows system, and reusability
for building a NaadCore digital harmonium library.

**Total Analysis Code**: 694 lines in main.js (no other application logic)
**External Dependency**: spessasynth_lib@4.3.0 (SF2 synthesizer)
**SoundFont**: harmonium.sf2 (loaded at runtime)

---

## FOCUS 1 — MIDI IMPLEMENTATION

### COMPLETE MIDI PATH

```
MIDI Device
    ↓ (WebMIDI API - navigator.requestMIDIAccess)
Web MIDI Access
    ↓ (onmidimessage event)
handleMidiMessage() - main.js:286-298
    ↓ (parse [status, note, velocity])
MIDI Message Parsing
    ↓ (calculate harmoniumIdx = note - 36)
harmonium note processing
    ↓ (handleKeyPress / handleKeyRelease)
note event handling
    ↓ (synth.noteOn / synth.noteOff)
Spessasynth WorkletSynthesizer
    ↓ (AudioWorklet)
Audio Output
```

### MIDI MESSAGE PARSING

**File**: `main.js:286-298`

```javascript
function handleMidiMessage(event) {
    const [status, note, velocity] = event.data;
    const isNoteOn = (status & 0xf0) === 0x90;
    const isNoteOff = ((status & 0xf0) === 0x80) || (isNoteOn && velocity === 0);
    const vel = velocity / 127;
    const harmoniumIdx = note - 36; 

    if (isNoteOn && vel > 0) {
        handleKeyPress(harmoniumIdx, vel);
    } else if (isNoteOff) {
        handleKeyRelease(harmoniumIdx);
    }
}
```

**Analysis**:
- **Channel handling**: ALL messages on ALL channels processed (no filtering)
- **MIDI note mapping**: MIDI notes 36-83 → harmonium indices 0-47
- **Velocity**: Normalized to 0.0-1.0 range
- **Note Off detection**: Handles both explicit 0x80 messages and Note On with velocity=0

### MIDI CHANNEL HANDLING

**Current Implementation**:
- Channel hardcoded to `0` in all operations (lines 348, 373, 376, 379, etc.)
- No configurable channel selection
- No MIDI channel filtering in `handleMidiMessage`

**Code Evidence**:
```javascript
// All operations use channel 0:
synth.noteOn(0, targetMidi, midiVelocity);
synth.noteOff(0, d.playedMidi);
synth.controllerChange(0, 7, midiVol);
synth.controllerChange(0, 11, expression);
synth.controllerChange(0, 64, sustainValue);
synth.controllerChange(0, 72, decayValue);
synth.controllerChange(0, 91, reverbValue);
synth.controllerChange(0, 93, chorusValue);
```

**Issues**:
1. No way to receive MIDI on different channels with different behaviors
2. All devices on all channels processed identically
3. Cannot route different instruments to different channels

**Recommendation for NaadCore**:
- Make channel configurable (default: 0)
- Add optional channel filter in MIDI handler
- Consider multi-channel support for complex setups

### VELOCITY HANDLING

**Conversion Chain**:
```javascript
// From MIDI input (main.js:290):
const vel = velocity / 127;  // 0-127 → 0.0-1.0

// In startAudio (main.js:346):
const midiVelocity = Math.floor(vel * 127);  // 0.0-1.0 → 0-127
```

**Problem**: Velocity rounding error with `Math.floor`

Example: MIDI velocity = 1
- `vel = 1 / 127 = 0.00787...`
- `midiVelocity = Math.floor(0.00787 * 127) = Math.floor(0.999...) = 0`

**Result**: MIDI velocity 1 produces no sound!

**Fix**: Use `Math.round(vel * 127)` instead of `Math.floor`

### DUPLICATE NOTES

**Handling Logic**:

1. **At `handleKeyPress` (line 325)**:
```javascript
if (activeNotes.has(i)) stopAudio(i);  // Stop existing before start
startAudio(i, vel);
```

2. **At `startAudio` (line 342)**:
```javascript
if (activeNotes.has(i) || !synth) return;  // Already playing
```

**Behavior**: Repetitions are retriggers (stop → start), not stacking.

**Drone Mode Exception** (line 315-322):
```javascript
if (isDroneMode && activeNotes.has(i)) {
    stopAudio(i);
    heldKeys.add(i);
    heldKeys.delete(i);  // Redundant - immediately removes
    return;
}
```

**Analysis**: The `heldKeys.add` then `heldKeys.delete` is redundant and should be removed.

### SIMULTANEOUS NOTES & POLYPHONY

**Tracking System**:

```javascript
activeNotes = new Map();    // Stores: index → {playedMidi, vel, couplerActive, subOctActive}
heldKeys = new Set();       // Currently pressed keys (keyboard input)
sustainQueue = new Set();   // Notes waiting during sustain (currently unused!)
```

**Voice Limit**: No explicit limit in code. Spessasynth handles internally (typically 64 voices).

**Too Many Notes**: When synth runs out of voices, oldest notes may be stolen automatically.

**Critical Insight**: Only original notes tracked in `activeNotes`. Coupled notes (±12 semitones)
are sent but NOT added to the map - they exist only as MIDI events.

### COUPLER AND SUB-OCTAVE

**Implementation** (main.js:350-356):
```javascript
synth.noteOn(0, targetMidi, midiVelocity);        // Original note

if (isCoupler) {
    synth.noteOn(0, targetMidi + 12, midiVelocity);  // +1 octave
}

if (isSubOct) {
    synth.noteOn(0, targetMidi - 12, midiVelocity);  // -1 octave
}

// State stored (line 358-363):
activeNotes.set(i, {
    playedMidi: targetMidi,
    vel: vel,
    couplerActive: isCoupler,    // Flag, not separate entry
    subOctActive: isSubOct       // Flag, not separate entry
});
```

**No separate voice tracking** for coupled notes:
- Coupled notes not in `activeNotes` map
- Coupler/sub-octave status stored as boolean flags
- UI cannot show separate state for coupled notes

**Update Logic** (main.js:395-414):
```javascript
activeNotes.forEach((d, i) => {
    if (isCoupler && !d.couplerActive) {
        synth.noteOn(0, d.playedMidi + 12, midiVelocity);
        d.couplerActive = true;
    } else if (!isCoupler && d.couplerActive) {
        synth.noteOff(0, d.playedMidi + 12);
        d.couplerActive = false;
    }
    // Similar for sub-octave
});
```

**Stop Logic** (main.js:375-380):
```javascript
if (d.couplerActive) {
    synth.noteOff(0, d.playedMidi + 12);
}
if (d.subOctActive) {
    synth.noteOff(0, d.playedMidi - 12);
}
```

**Issues**:
1. All three notes (original, +12, -12) played at same velocity
2. No individual voice management for coupled notes
3. If settings change while note playing, update happens but may cause audio artifacts

### SUSTAIN IMPLEMENTATION

**Queue System** (main.js:333-335):
```javascript
if (isSustain) {
    sustainQueue.add(i);  // Added but never read!
    return;
}
```

**Problem**: `sustainQueue` is populated but NEVER USED. Notes remain playing via `activeNotes` Map.

**Actual Sustain Release Logic** (main.js:550-556):
```javascript
if(!s){
    isDroneMode = false;  // ⚠️ FORCES drone mode off!
    document.getElementById("hold-btn")?.classList.remove("active");
    activeNotes.forEach((_, i) => { 
        if (!heldKeys.has(i)) stopAudio(i);  // Only stop notes NOT currently held
    });
    sustainQueue.clear();  // Clear unused queue
}
```

**Critical Bug**: Sustain release (line 551) **forces drone mode off**. User may not expect this behavior.

**Analysis**: 
- Sustain system works but has design issues
- `sustainQueue` is dead code
- Drone mode cancellation is too aggressive

### MIDI CC CONTROLLERS

**Implemented CCs** (Spessasynth handles these):

| CC# | Controller | Purpose | Line |
|-----|------------|---------|------|
| 7 | Main Volume | Master volume | 211 |
| 11 | Expression | Bellows pressure | 569 |
| 64 | Sustain Pedal | Note hold | 548 |
| 72 | Decay Time | Release envelope | 223 |
| 91 | Reverb Depth | Effects | 229 |
| 93 | Chorus Depth | Effects | 236 |

**All CCs sent via**: `synth.controllerChange(channel, ccNumber, value)`

---

## FOCUS 2 — POLYPHONY ANALYSIS

### NOTE TRACKING

**Data Structures**:

```javascript
// Line 18-20:
const activeNotes = new Map(),    // index → note state
      heldKeys = new Set(),       // currently pressed keys
      sustainQueue = new Set();   // unused queue
```

**Note Identity Tracking**:

```javascript
// startAudio stores (line 358-363):
activeNotes.set(i, { 
    playedMidi: targetMidi,           // MIDI note number
    vel: vel,                         // Original velocity
    couplerActive: isCoupler,         // Flag for +12
    subOctActive: isSubOct            // Flag for -12
});
```

**Note Off Association**:

```javascript
// stopAudio retrieves (line 370-380):
const d = activeNotes.get(i);
synth.noteOff(0, d.playedMidi);  // Uses stored playedMidi
if (d.couplerActive) {
    synth.noteOff(0, d.playedMidi + 12);
}
if (d.subOctActive) {
    synth.noteOff(0, d.playedMidi - 12);
}
```

**Key Insight**: `activeNotes.get(i)` returns the exact MIDI note to turn off, not just the index.

### COUPLER/SUB-OCTAVE INTERACTION

**Scenario**: Original note playing, then toggle coupler

1. `isCoupler = true` (toggle UI)
2. `refreshAudio()` called (line 192-194)
3. For each active note:
   ```javascript
   if (isCoupler && !d.couplerActive) {
       synth.noteOn(0, d.playedMidi + 12, midiVelocity);
       d.couplerActive = true;
   }
   ```

**Result**: New note added, original continues, stored state updated.

**Scenario**: Toggle off while note playing

```javascript
if (!isCoupler && d.couplerActive) {
    synth.noteOff(0, d.playedMidi + 12);
    d.couplerActive = false;
}
```

**Result**: Coupled note stopped, original continues.

**Potential Issues**:
1. **No voice limit**: If playing 4 notes + coupler + sub-oct = 12 voices
2. **Same MIDI note**: If transpose causes overlap, may cause voice conflicts
3. **Velocity mismatch**: All three notes use same velocity (not realistic)

### DUPLICATE MIDI NOTES

**Scenario**: Two different harmonium keys produce same MIDI note

Example:
- Key 0 (MIDI 36) + transpose +12 = MIDI 48
- Key 12 (MIDI 48) = MIDI 48

**Current behavior**:
```javascript
// startAudio checks:
if (activeNotes.has(i) || !synth) return;  // Different indices don't conflict
```

**Result**: Both notes play independently (different `i` values), even if same MIDI note.

**Potential stuck notes**: If one key released but other still pressed:
```javascript
// stopAudio only stops note for index i
activeNotes.delete(i);  // Only removes this key's entry
```

**Actually Safe**: Each harmonium key has its own entry, so no stuck notes from this scenario.

---

## FOCUS 3 — HARMONIUM SOUND

### SOUNDFont Analysis

**File**: `harmonium.sf2` (root directory)

**Loading** (main.js:628-631):
```javascript
const sfont = await (await fetch("./harmonium.sf2")).arrayBuffer();
synth = new WorkletSynthesizer(audioCtx);
await synth.soundBankManager.addSoundBank(sfont, "main");
await synth.isReady;
```

**Program Change** (line 647):
```javascript
synth.programChange(0, 0);  // Bank 0, Program 0
```

**Spessasynth Capabilities** (v4.3.0):

From external analysis (GitHub repo):

✅ **Full SoundFont 2.04 support**
✅ **SoundFont 3.0 (SF3) support**
✅ **DLS Level 1 & 2 support**
✅ **Full generator/modulator system**
✅ **Effects**: Reverb, Chorus, Delay
✅ **MIDI CC support**: All standard controllers
✅ **Per-note pitch wheel**
✅ **Unlimited polyphony** (CPU-limited)

**Unknown** (cannot inspect .sf2 file):
- Number of instruments/presets
- Sample count and quality
- Velocity layers present
- Loop points configuration
- Envelope parameters (ADSR)
- Whether samples are actually harmonium

### ACTUAL SOUND GENERATION

**Spessasynth Processing Chain**:
```
MIDI noteOn → Sample lookup → Interpolation → ADSR envelope → 
Filter → Effects → Output

1. Sample lookup by MIDI note
2. Velocity-based sample selection (if velocity zones defined)
3. Sample playback with interpolation
4. Envelope application (attack, decay, sustain, release)
5. Filter modulation (cutoff, resonance)
6. Effects (reverb, chorus, delay)
7. Final mix to output
```

**Does velocity affect timbre?**

- **Via SF2 design**: If SoundFont has velocity layers, different samples play at different velocities
- **Via envelopes**: Velocity affects attack time (in SF2 generators)
- **Via amplitude**: Higher velocity = louder (always)

**Harmonium authenticity concerns**:
- Real harmonium: Bellows pressure = expression, not velocity
- Digital implementation: Velocity sets initial amplitude, bellows sustains
- **This is correct behavior** for harmonium emulation

### ENVELOPE AND EFFECTS

**Default ADSR** (from Spessasynth, not in this code):
- Attack: Configurable via CC#73
- Decay: Configurable via CC#75  
- Sustain: Configurable via SF2 generator
- Release: Configurable via CC#72 (line 223)

**CC#72 Implementation** (main.js:215-224):
```javascript
document.getElementById('sus')?.addEventListener("input", e => {
    // Slider labeled "Sustain" but affects Release time!
    const norm = (val - minSliderVal) / (maxSliderVal - minSliderVal);
    const midiVal = Math.floor(norm * 127);
    synth.controllerChange(0, 72, midiVal);  // CC#72 = Release Time
});
```

**Bug**: UI says "Sustain" but controls **Release** (CC#72). Confusing naming.

**Effects** (CC#91, CC#93):
```javascript
// Reverb (line 226-230):
synth.controllerChange(0, 91, Math.floor(val * 127));

// Chorus (line 232-237):
synth.controllerChange(0, 93, Math.floor((val / maxSliderVal) * 127));
```

---

## FOCUS 4 — AUDIO ENGINE

### AUDIO WORKLET

**Loading** (main.js:620-626):
```javascript
const processorUrl = "https://unpkg.com/spessasynth_lib@4.3.0/dist/spessasynth_processor.min.js";
const processorCode = await (await fetch(processorUrl)).text();
const blob = new Blob([processorCode], { type: "application/javascript" });
const blobUrl = URL.createObjectURL(blob);

await audioCtx.audioWorklet.addModule(blobUrl);
URL.revokeObjectURL(blobUrl);
```

**Method**: Dynamic blob loading (avoiding CORS issues with direct URL)

**Spessasynth AudioWorklet**:
- Processes 128-sample blocks
- Runs in dedicated audio thread
- Low latency (~3ms at 48kHz)

### AUDIO GRAPH

**Configuration** (main.js:633-644):
```javascript
// Main path:
synth → volumeNode → destination

// Visualizer path:
synth → analyser → volumeNode → destination
```

**Nodes**:
1. **WorkletSynthesizer** (synth): Audio source from Spessasynth
2. **GainNode** (volumeNode): Master volume control
3. **AnalyserNode**: FFT analysis for visualizer (fftSize: 1024)

**Signal Flow**:
```
synth output
    ├─→ volumeNode → destination (main output)
    └─→ analyser → volumeNode → destination (visualizer)
```

**Note**: Analyser is placed BEFORE final destination for visualizer, but AFTER gain.

### AUDIO BUFFER GENERATION

**Location**: Spessasynth AudioWorklet (external library)

**Process**:
1. Spessasynth processor receives MIDI messages via postMessage
2. AudioWorklet renders 128-sample blocks
3. Voices synthesized, mixed, effects applied
4. Output to main bus and visualizer bus

**Voice Creation/Mixing**: All handled internally by Spessasynth

**Envelope Application**: Spessasynth handles ADSR envelopes

**Effects Application**: Spessasynth handles reverb/chorus (configured via CC)

### LATENCY CONSIDERATIONS

**Processing Latency**: ~3ms (128 samples @ 48kHz)

**Total System Latency**:
- AudioWorklet block: ~2.7ms
- Message passing: <0.1ms
- Web MIDI input: <1ms
- **Total**: ~3-4ms (excellent for real-time play)

**Browser Assumptions**:
- Works with default AudioContext sample rate (44.1kHz or 48kHz)
- Assumes user interaction before AudioContext start (required by browsers)
- Assumes CORS access to worklet processor URL

---

## FOCUS 5 — BELLOW/EXPRESSION SYSTEM

### RESERVOIR MODEL

**Physics** (main.js:558-579):
```javascript
function loop() {
    if (isManual) {
        // Air added via pump
        const targetFill = Math.min(100, reservoir + pumpCharge); 
        reservoir += (targetFill - reservoir) * 0.04;  // Charging (4% per frame)
        pumpCharge *= 0.95;  // Pump charge decay
        
        // Air drains based on active notes
        const drain = 0.02 + activeNotes.size * 0.03;  // Base + per-note drain
        reservoir = Math.max(0, reservoir - drain);  // Apply drain
        
        // Update MIDI expression controller
        const expressionVal = Math.floor((reservoir / 100) * 127);
        const clampedExpression = Math.min(127, Math.max(0, expressionVal));
        synth.controllerChange(0, 11, clampedExpression);  // CC#11 = Expression
    }
    
    // Update UI meter
    const airFillEl = document.getElementById('air-fill');
    if (airFillEl) {
        airFillEl.style.width = reservoir + "%";
    }
    
    requestAnimationFrame(loop);
}
```

**Parameters**:
- `reservoir`: Current air volume (0-100)
- `pumpCharge`: Temporary air addition from pumping
- `drain`: 0.02 (base) + 0.03 per active note
- `expressionVal`: CC#11 value (0-127)

**Behavior**:

1. **Manual Mode** (`isManual = true`):
   - Air drains continuously
   - Spacebar adds pumpCharge (line 674)
   - Expression controller follows reservoir
   - If reservoir = 0, no sound (CC#11 = 0)

2. **Auto Mode** (`isManual = false`):
   - Reservoir stays at 100 (line 162: `reservoir = 100`)
   - CC#11 fixed at 127 (line 167)
   - No pumping needed

**UI Controls**:

```javascript
// Line 159-172: Toggle between auto/manual
manualToggle.addEventListener("change", e => {
    isManual = e.target.checked;
    reservoir = isManual ? 0 : 100;  // Reset reservoir on switch
    
    if (synth) {
        if (!isManual) {
            synth.controllerChange(0, 11, 127);  // Auto: full pressure
        } else {
            synth.controllerChange(0, 11, 0);  // Manual: start empty
        }
    }
});
```

### IS BELLOW SYSTEM NEEDED FOR NAADCORE?

**Current Goal**: "MIDI key velocity → natural harmonium response"

**Does NOT need**: "physical or virtual bellows system"

**Recommendation**:
- **REMOVE** reservoir system from core
- **KEEPS**: Expression via CC#11 (for UI compatibility)
- **Simplify**: Just map velocity to amplitude, no bellows physics

**Alternative Approach**:
```
MIDI velocity → amplitude (via SF2 velocity layers)
Key hold duration → sustain (via SF2 ADSR)
Expression slider → global volume (CC#7 or CC#11)
```

This is simpler, more efficient, and matches the actual harmonium feel better.

---

## FOCUS 6 — COUPLER / SUB-OCTAVE

### IMPLEMENTATION LOCATION

**Current**: main.js lines 350-356, 375-380, 398-414

**Classification**:

**Option A: Generic Core** ❌
- Coupler/sub-octave are instrument-specific features
- Not all instruments have these
- Should be harmonium-specific

**Option B: Harmonium Module** ✅
- Signature harmonium features
- Common on Indian harmoniums
- Should be in harmonium-specific layer

**Option C: UI/Application Layer** ❌
- The logic modifies audio behavior (note ±12)
- Not just UI state
- Should be in instrument module

**Best Classification**: **HARMONIUM MODULE**

**Reasoning**:
1. Coupler is harmonium-specific (extra pipe bank)
2. Sub-octave is harmonium-specific (lower pipe bank)
3. Both affect note generation (multiple MIDI notes)
4. Should be configurable per-instrument

**Proposed Architecture**:

```javascript
// harmonium-core.js (NEW)
class HarmoniumSynthesizer {
    constructor(synth) {
        this.synth = synth;  // Spessasynth instance
        this.isCoupler = false;
        this.isSubOct = false;
    }
    
    noteOn(channel, midiNote, velocity) {
        this.synth.noteOn(channel, midiNote, velocity);
        if (this.isCoupler) {
            this.synth.noteOn(channel, midiNote + 12, velocity);
        }
        if (this.isSubOct) {
            this.synth.noteOn(channel, midiNote - 12, velocity);
        }
    }
    
    toggleCoupler(enable) {
        this.isCoupler = enable;
        this.refreshActiveNotes();
    }
    
    toggleSubOctave(enable) {
        this.isSubOct = enable;
        this.refreshActiveNotes();
    }
}

// main.js (UI layer)
const harmoniumSynth = new HarmoniumSynthesizer(synth);
document.getElementById('coupler-toggle').addEventListener('change', e => {
    harmoniumSynth.toggleCoupler(e.target.checked);
});
```

**Benefits**:
- Separates harmonium-specific logic from MIDI handling
- Reusable for other instruments
- Cleaner code organization

---

## FOCUS 7 — REUSABILITY CLASSIFICATION

### REUSABLE CORE (For NaadCore Library)

| Functionality | File/Location | Class/Component |
|---------------|---------------|-----------------|
| **MIDI abstraction** | main.js:286-298 | `handleMidiMessage()` |
| **Note events** | main.js:300-339 | `handleKeyPress()`, `handleKeyRelease()` |
| **Voice management** | main.js:341-385 | `startAudio()`, `stopAudio()` |
| **Polyphony tracking** | main.js:18-20 | `activeNotes`, `heldKeys` |
| **Sample playback** | Spessasynth (external) | `WorkletSynthesizer` |
| **Mixing** | Spessasynth (external) | AudioWorklet processor |
| **Effects** | Spessasynth (external) | Reverb/Chorus via CC |
| **Audio output** | main.js:633-644 | Audio graph construction |

**Spessasynth API to Expose**:
```javascript
// Core minimal API
synth.noteOn(channel, midiNote, velocity);
synth.noteOff(channel, midiNote);
synth.controllerChange(channel, ccNumber, value);
synth.programChange(channel, programNumber);
synth.soundBankManager.addSoundBank(buffer, name);
await synth.isReady;
```

### HARMONIUM MODULE (Instrument-Specific)

| Feature | File/Location | Implementation |
|---------|---------------|----------------|
| **harmonium samples** | harmonium.sf2 | SoundFont file |
| **tuning** | transposeShift | main.js:417-425 |
| **coupler** | main.js:350-356 | note+12 logic |
| **sub-octave** | main.js:354-356 | note-12 logic |
| **drones** | main.js:197-205 | isDroneMode logic |
| **harmonium expression** | main.js:558-579 | Reservoir system (REMOVE from core) |
| **harmonium behavior** | main.js:300-339 | Raga filtering |

**Harmonium Module API**:
```javascript
// harmonium-module.js (NEW)
class IndianHarmonium {
    constructor(synth) {
        this.synth = synth;
        this.transpose = 0;
        this.isCoupler = false;
        this.isSubOct = false;
        this.ragaScale = null;
    }
    
    noteOn(index, velocity) {
        let midiNote = 36 + index + this.transpose;
        this.synth.noteOn(0, midiNote, velocity);
        if (this.isCoupler) {
            this.synth.noteOn(0, midiNote + 12, velocity);
        }
        if (this.isSubOct) {
            this.synth.noteOn(0, midiNote - 12, velocity);
        }
    }
    
    setRaga(scaleArray) {
        this.ragaScale = scaleArray;
    }
    
    validateNote(index) {
        if (!this.ragaScale) return true;
        const note = (index + this.transpose) % 12;
        return this.ragaScale.includes(note);
    }
}
```

### UI / APPLICATION (Current Implementation)

| Component | File | Description |
|-----------|------|-------------|
| **HTML** | index.html | UI structure |
| **CSS** | style.css | Styling (1152 lines) |
| **Visual keyboard** | main.js:57-156 | Keyboard rendering |
| **Raga UI** | main.js:507-543 | Raga selector, highlighting |
| **Notation UI** | main.js:454-505 | Sargam/Western toggle |
| **Visualizer** | main.js:583-615 | Waveform display |
| **Buttons/Sliders** | main.js:158-249 | Control handlers |

### EXTERNAL DEPENDENCY

| Library | Purpose | Usage |
|---------|---------|-------|
| **Spessasynth** | SF2 synthesizer | Audio generation |
| **Web Audio API** | Browser audio | AudioContext, Worklet |
| **Web MIDI API** | MIDI I/O | navigator.requestMIDIAccess |

---

## FOCUS 8 — NAADCORE EXTRACTION

### PROPOSED BOUNDARY

### WHAT CAN BE REALISTICALLY EXTRACTED

1. **Spessasynth Wrapper** ✅
   - Minimal API: noteOn, noteOff, controllerChange, programChange
   - SoundBank loading
   - State management for active notes

2. **MIDI Message Parser** ✅
   - Parse status byte → Note On/Off
   - Velocity normalization
   - Channel handling

3. **Note Tracking System** ✅
   - Map-based activeNotes
   - Velocity caching
   - Coupler/sub-oct flags

4. **Raga System** ✅
   - Scale definitions
   - Note validation
   - Highlighting logic

### WHAT SHOULD NOT BE COPIED

1. **Reservoir Bellows Physics** ❌
   - Application-specific (not needed per requirements)
   - Complex physics not essential for core
   - Remove from NaadCore

2. **UI Code** ❌
   - HTML/CSS (1152 lines)
   - Keyboard rendering
   - Visualizer
   - All DOM manipulation

3. **Transpose Logic** ⚠️
   - Keep basic transpose
   - Remove raga-related aspects
   - Simplify to pure transposition

### WHAT SHOULD REMAIN DEPENDENCY

1. **Spessasynth** ✅
   - Essential SF2 playback
   - Complex audio engine
   - Maintain as external dependency

2. **Web Audio API** ✅
   - Native browser API
   - Not worth re-implementing

3. **Web MIDI API** ✅
   - Native browser API
   - Standard interface

### WHAT WOULD HAVE TO BE REWRITTEN FOR LINUX

**None** - This is a web-based application using Web Audio/MIDI APIs.

For Linux desktop app, consider:
- Electron with Web Audio/MIDI (keeps most code)
- Native Rust/C++ with PortAudio + RtMIDI
- WebAssembly port of Spessasynth

### TIGHTLY COUPLED TO BROWSER

1. **DOM manipulation** (main.js:57-156, 651-656)
2. **CSS styling** (style.css)
3. **Event listeners** (onkeydown, onkeyup, onmousedown)
4. **Canvas rendering** (drawVisualizer)
5. **Web MIDI API** (navigator.requestMIDIAccess)

**Note**: Web MIDI API is browser standard, not proprietary.

### GENUINELY INSTRUMENT-INDEPENDENT

1. **MIDI parsing** (main.js:286-298)
2. **Note on/off handling** (main.js:300-339)
3. **Active notes tracking** (main.js:18-20)
4. **Velocity handling** (main.js:290, 346)
5. **CC controller routing** (main.js:210-237)
6. **Raga system** (main.js:32-48, 519-543) - music theory, not harmonium-specific

---

## FINAL CLASSIFICATION

### A. KEEP (Move to NaadCore)

| Component | File | Lines | Reason |
|-----------|------|-------|--------|
| MIDI message parser | main.js | 286-298 | Essential, reusable |
| Note event handling | main.js | 300-339 | Core functionality |
| Active notes Map | main.js | 18-20 | Polyphony tracking |
| Velocity conversion | main.js | 290, 346 | Standard MIDI |
| Raga system | main.js | 32-48, 519-543 | Music theory |
| Note retrigger | main.js | 325 | Playability |

### B. REWRITE (Refactor for Core)

| Component | File | Lines | Action |
|-----------|------|-------|--------|
| Reservoir bellows | main.js | 558-579 | Remove or simplify |
| Transpose logic | main.js | 417-425 | Simplify, remove raga coupling |
| Coupler/sub-octave | main.js | 350-415 | Move to harmonium module |
| Drone mode | main.js | 315-322, 197-205 | Simplify, make optional |
| Sustain queue | main.js | 333-335, 545-556 | Fix or remove |

### C. REUSE AS DEPENDENCY

| Component | Location | Version | Type |
|-----------|----------|---------|------|
| Spessasynth | unpkg.com | 4.3.0 | Library |
| Web Audio API | Browser | Native | API |
| Web MIDI API | Browser | Native | API |

### D. DISCARD (Remove from Core)

| Component | File | Lines | Reason |
|-----------|------|-------|--------|
| Visualizer code | main.js | 583-615 | UI only |
| Keyboard rendering | main.js | 57-156 | UI only |
| HTML/CSS | index.html, style.css | All | UI only |
| DOM event handlers | main.js:670-689 | UI only |
| UI button setup | main.js:158-249 | UI only |

### E. NEEDS FURTHER INVESTIGATION

| Component | Issue | Investigation Needed |
|-----------|-------|---------------------|
| harmonium.sf2 | Quality, samples | Inspect with SF2 tool |
| Velocity rounding | Math.floor vs round | Test with actual MIDI |
| Sustain logic | Confusing naming | Clarify CC#72 vs sustain |
| MIDI channel | Hardcoded to 0 | Should be configurable |
| heldKeys duplicate | Lines 319-320 | Remove redundant code |

---

## RECOMMENDATIONS FOR NAADCORE

### Phase 1: Extract Minimal Core (2-3 files)

**File 1: `naad-core-midi.js`** (100-150 lines)
- MIDI message parsing
- Note on/off handling
- Active notes Map
- Basic velocity handling
- CC controller routing

**File 2: `naad-core-harmonium.js`** (100-150 lines)
- Indian harmonium-specific features
- Transpose with raga support
- Coupler/sub-octave
- Drone mode
- Simplified expression (remove reservoir)

**File 3: `naad-synth-wrapper.js`** (50-80 lines)
- Spessasynth integration
- SoundBank loading
- Minimal API surface

### Phase 2: Remove Application-Specific Code

- Delete all UI code (DOM, CSS, keyboard rendering)
- Remove visualizer
- Remove raga highlighting logic
- Keep only raga scale definitions and validation

### Phase 3: Add Missing Features

- MIDI channel configuration
- Voice limit tracking
- Better sustain implementation
- Velocity rounding fix
- Error handling

### Phase 4: Documentation & Testing

- API documentation
- Unit tests for MIDI parsing
- Integration tests with Spessasynth
- Example harmonium application

---

## CRITICAL BUGS TO FIX BEFORE EXTRACTION

1. **Velocity rounding** (main.js:346)
   ```javascript
   // Current:
   const midiVelocity = Math.floor(vel * 127);
   
   // Fix:
   const midiVelocity = Math.round(vel * 127);
   ```

2. **Sustain cancels drone** (main.js:551)
   ```javascript
   // Remove this line - too aggressive
   isDroneMode = false;
   ```

3. **Unused sustainQueue** (main.js:334)
   ```javascript
   // Either implement properly or remove
   ```

4. **heldKeys duplicate** (main.js:319-320)
   ```javascript
   // Remove these redundant lines:
   heldKeys.add(i);
   heldKeys.delete(i);
   ```

5. **CC#72 labeled "Sustain"** (main.js:215-224)
   ```javascript
   // UI says "Sustain" but controls Release time (CC#72)
   // Either fix label or implement actual sustain
   ```

---

## CONCLUSION

The harmonium-companion codebase is well-structured with 694 lines of application logic
separated from 3rd-party Spessasynth synthesis. For NaadCore, extract:

✅ **KEEP**: MIDI handling, note tracking, raga system, velocity logic
⚠️ **REWRITE**: Reservoir (remove), transpose, coupler/sub-octave (move to harmonium module)
❌ **DISCARD**: All UI code (DOM, CSS, rendering)
📦 **DEPENDENCY**: Spessasynth, Web Audio, Web MIDI

**Target**: 250-300 lines of reusable core, plus harmonium-specific module.

**Timeline**: 2-3 weeks for minimal viable core, 4-6 weeks for complete harmonium module.
