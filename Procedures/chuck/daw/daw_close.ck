// Placeholder DAW close cue.
TriOsc tone => LPF lp => Gain g => dac.chan(0);
0.16 => g.gain;
1800.0 => lp.freq;
0.7 => tone.gain;

[760.0, 560.0, 420.0] @=> float notes[];
for (0 => int i; i < notes.size(); i++) {
    notes[i] => tone.freq;
    20::ms => now;
}

0.0 => g.gain;
