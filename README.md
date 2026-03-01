Custom builtin Command: thuum

The thuum command is a Skyrim-inspired custom builtin command where you as a dragonborn learn to shout words of power in the realm of Apocrypha.

Each dragon word has:

A dragon form (e.g., FUS)

An English meaning (e.g., “Force”)

A learned state (stored in memory)

Words must be learned before their meanings are revealed.

Features:

22 predefined dragon words stored in an internal dictionary named apocrypha

Learning system using --learn

Listing learned words using --known

Immersive Hermaeus Mora–style responses

Session-based memory (resets when program exits)

Usage:

Speak a Word:

thuum WORD

Learn a Word:

thuum --learn WORD

List Learned Words:

thuum --known

Available Words of Power

The following dragon words are available in Apocrypha and may be learned during a session:

    {"FUS", "Force", 0},
    {"RO",  "Balance", 0},
    {"DAH", "Push", 0},

    {"YOL",  "Fire", 0},
    {"TOOR", "Inferno", 0},
    {"FO",   "Frost", 0},
    {"KRAH", "Cold", 0},

    {"ZUL",  "Voice", 0},
    {"TIID", "Time", 0},

    {"WULD", "Whirlwind", 0},
    {"NAH",  "Fury", 0},
    {"KEST", "Tempest", 0},
    {"LAAS", "Life", 0},
    {"YAH",  "Seek", 0},
    {"NIR",  "Hunt", 0},
    {"KREN", "Banish", 0},
    {"DAAN", "Doom", 0},
    {"JOOR", "Mortal", 0},
    {"ZAH",  "Finite", 0},
    {"FRII", "Freeze", 0},
    {"SU",   "Air", 0},
    {"GRAH", "Battle", 0}

