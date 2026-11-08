# Sword Ember special attack

Imported from the user-provided Pierre02/Sword_Ember.efkproj.
The editable source and all 11 referenced resources (nine textures and two sword models)
are kept together in this directory; do not omit Texture/ or Model/ when sharing the game.
The effect runtime file was validated in-game, including color/distortion images and models.

Both players use this effect for a NEW energy attack, independent of the original SwordMega special.
Playback: 3x source speed. Scale: 20. Maximum lifetime: 300 fixed ticks (5 seconds).
Cast movement lock: 165 ticks (2.75 seconds); the effect stays at the original cast position.
Source slash events at frames 127/180/240/271 map to ticks 43..100.
The source finisher at frame 450 maps to tick 150. Damage is disabled during charging.
Damage stays at the existing two HP per target, once per cast.

The NEW attack uses P1 R / P2 Numpad5 / each controller R3, requires 100 energy, and clears
that player's energy only after successful playback starts. Match restart resets both bars.
The original special remains P1 F / P2 Numpad0 / controller Y, and does not consume energy.
Normal attack skill cancels remain available.

Validation: --player-smoke --ember-preview checks resources, both players, single energy
consumption, damage timing/range, anchored origin, movement recovery and cleanup.
Preview frames are saved in work/ember-preview-{43,90,150,170}.png.

Export from this directory using the installed Effekseer CLI:
Effekseer.exe -cui -in Sword_Ember.efkproj -o Sword_Ember.efkefc

Normal energy mode restored: PLAYER_EMBER_FREE_CAST_TEST=false. Casting requires 100 energy and clears the gauge only after a successful start.

Energy attack actor motion: great sword power up.mv1, converted animation-only from the existing FBX with DxLibModelViewer. The clip plays once across the 165-tick cast lock; the original special keeps high spin attack.
