#ifndef POKEPLATINUM_PORT_GEN5_MOVES_TABLE_H
#define POKEPLATINUM_PORT_GEN5_MOVES_TABLE_H

#include <nitro/types.h>

#include "generated/pokemon_types.h"

#define GEN5_MOVE_FIRST 468
#define GEN5_MOVE_LAST  559

typedef struct Gen5MoveEntry {
    const char *name;
    u8 type;
    u8 power;
    u8 accuracy;
    u8 category; // Platinum class: 0 = Physical, 1 = Special, 2 = Status
} Gen5MoveEntry;

static const Gen5MoveEntry sGen5Moves[GEN5_MOVE_LAST - GEN5_MOVE_FIRST + 1] = {
    { "Hone Claws",    TYPE_DARK,     0,   0,   2 }, // 468
    { "Wide Guard",    TYPE_ROCK,     0,   0,   2 }, // 469
    { "Guard Split",   TYPE_PSYCHIC,  0,   0,   2 }, // 470
    { "Power Split",   TYPE_PSYCHIC,  0,   0,   2 }, // 471
    { "Wonder Room",   TYPE_PSYCHIC,  0,   0,   2 }, // 472
    { "Psyshock",      TYPE_PSYCHIC,  80,  100, 1 }, // 473
    { "Venoshock",     TYPE_POISON,   65,  100, 1 }, // 474
    { "Autotomize",    TYPE_STEEL,    0,   0,   2 }, // 475
    { "Rage Powder",   TYPE_BUG,      0,   0,   2 }, // 476
    { "Telekinesis",   TYPE_PSYCHIC,  0,   0,   2 }, // 477
    { "Magic Room",    TYPE_PSYCHIC,  0,   0,   2 }, // 478
    { "Smack Down",    TYPE_ROCK,     50,  100, 0 }, // 479
    { "Storm Throw",   TYPE_FIGHTING, 40,  100, 0 }, // 480
    { "Flame Burst",   TYPE_FIRE,     70,  100, 1 }, // 481
    { "Sludge Wave",   TYPE_POISON,   95,  100, 1 }, // 482
    { "Quiver Dance",  TYPE_BUG,      0,   0,   2 }, // 483
    { "Heavy Slam",    TYPE_STEEL,    0,   100, 0 }, // 484
    { "Synchronoise",  TYPE_PSYCHIC,  70,  100, 1 }, // 485
    { "Electro Ball",  TYPE_ELECTRIC, 0,   100, 1 }, // 486
    { "Soak",          TYPE_WATER,    0,   100, 2 }, // 487
    { "Flame Charge",  TYPE_FIRE,     50,  100, 0 }, // 488
    { "Coil",          TYPE_POISON,   0,   0,   2 }, // 489
    { "Low Sweep",     TYPE_FIGHTING, 60,  100, 0 }, // 490
    { "Acid Spray",    TYPE_POISON,   40,  100, 1 }, // 491
    { "Foul Play",     TYPE_DARK,     95,  100, 0 }, // 492
    { "Simple Beam",   TYPE_NORMAL,   0,   100, 2 }, // 493
    { "Entrainment",   TYPE_NORMAL,   0,   100, 2 }, // 494
    { "After You",     TYPE_NORMAL,   0,   0,   2 }, // 495
    { "Round",         TYPE_NORMAL,   60,  100, 1 }, // 496
    { "Echoed Voice",  TYPE_NORMAL,   40,  100, 1 }, // 497
    { "Chip Away",     TYPE_NORMAL,   70,  100, 0 }, // 498
    { "Clear Smog",    TYPE_POISON,   50,  0,   1 }, // 499
    { "Stored Power",  TYPE_PSYCHIC,  20,  100, 1 }, // 500
    { "Quick Guard",   TYPE_FIGHTING, 0,   0,   2 }, // 501
    { "Ally Switch",   TYPE_PSYCHIC,  0,   0,   2 }, // 502
    { "Scald",         TYPE_WATER,    80,  100, 1 }, // 503
    { "Shell Smash",   TYPE_NORMAL,   0,   0,   2 }, // 504
    { "Heal Pulse",    TYPE_PSYCHIC,  0,   0,   2 }, // 505
    { "Hex",           TYPE_GHOST,    50,  100, 1 }, // 506
    { "Sky Drop",      TYPE_FLYING,   60,  100, 0 }, // 507
    { "Shift Gear",    TYPE_STEEL,    0,   0,   2 }, // 508
    { "Circle Throw",  TYPE_FIGHTING, 60,  90,  0 }, // 509
    { "Incinerate",    TYPE_FIRE,     30,  100, 1 }, // 510
    { "Quash",         TYPE_DARK,     0,   100, 2 }, // 511
    { "Acrobatics",    TYPE_FLYING,   55,  100, 0 }, // 512
    { "Reflect Type",  TYPE_NORMAL,   0,   0,   2 }, // 513
    { "Retaliate",     TYPE_NORMAL,   70,  100, 0 }, // 514
    { "Final Gambit",  TYPE_FIGHTING, 0,   100, 1 }, // 515
    { "Bestow",        TYPE_NORMAL,   0,   0,   2 }, // 516
    { "Inferno",       TYPE_FIRE,     100, 50,  1 }, // 517
    { "Water Pledge",  TYPE_WATER,    50,  100, 1 }, // 518
    { "Fire Pledge",   TYPE_FIRE,     50,  100, 1 }, // 519
    { "Grass Pledge",  TYPE_GRASS,    50,  100, 1 }, // 520
    { "Volt Switch",   TYPE_ELECTRIC, 70,  100, 1 }, // 521
    { "Struggle Bug",  TYPE_BUG,      30,  100, 1 }, // 522
    { "Bulldoze",      TYPE_GROUND,   60,  100, 0 }, // 523
    { "Frost Breath",  TYPE_ICE,      40,  90,  1 }, // 524
    { "Dragon Tail",   TYPE_DRAGON,   60,  90,  0 }, // 525
    { "Work Up",       TYPE_NORMAL,   0,   0,   2 }, // 526
    { "Electroweb",    TYPE_ELECTRIC, 55,  95,  1 }, // 527
    { "Wild Charge",   TYPE_ELECTRIC, 90,  100, 0 }, // 528
    { "Drill Run",     TYPE_GROUND,   80,  95,  0 }, // 529
    { "Dual Chop",     TYPE_DRAGON,   40,  90,  0 }, // 530
    { "Heart Stamp",   TYPE_PSYCHIC,  60,  100, 0 }, // 531
    { "Horn Leech",    TYPE_GRASS,    75,  100, 0 }, // 532
    { "Sacred Sword",  TYPE_FIGHTING, 90,  100, 0 }, // 533
    { "Razor Shell",   TYPE_WATER,    75,  95,  0 }, // 534
    { "Heat Crash",    TYPE_FIRE,     0,   100, 0 }, // 535
    { "Leaf Tornado",  TYPE_GRASS,    65,  90,  1 }, // 536
    { "Steamroller",   TYPE_BUG,      65,  100, 0 }, // 537
    { "Cotton Guard",  TYPE_GRASS,    0,   0,   2 }, // 538
    { "Night Daze",    TYPE_DARK,     85,  95,  1 }, // 539
    { "Psystrike",     TYPE_PSYCHIC,  100, 100, 1 }, // 540
    { "Tail Slap",     TYPE_NORMAL,   25,  85,  0 }, // 541
    { "Hurricane",     TYPE_FLYING,   120, 70,  1 }, // 542
    { "Head Charge",   TYPE_NORMAL,   120, 100, 0 }, // 543
    { "Gear Grind",    TYPE_STEEL,    50,  85,  0 }, // 544
    { "Searing Shot",  TYPE_FIRE,     100, 100, 1 }, // 545
    { "Techno Blast",  TYPE_NORMAL,   85,  100, 1 }, // 546
    { "Relic Song",    TYPE_NORMAL,   75,  100, 1 }, // 547
    { "Secret Sword",  TYPE_FIGHTING, 85,  100, 1 }, // 548
    { "Glaciate",      TYPE_ICE,      65,  95,  1 }, // 549
    { "Bolt Strike",   TYPE_ELECTRIC, 130, 85,  0 }, // 550
    { "Blue Flare",    TYPE_FIRE,     130, 85,  1 }, // 551
    { "Fiery Dance",   TYPE_FIRE,     80,  100, 1 }, // 552
    { "Freeze Shock",  TYPE_ICE,      140, 90,  0 }, // 553
    { "Ice Burn",      TYPE_ICE,      140, 90,  1 }, // 554
    { "Snarl",         TYPE_DARK,     55,  95,  1 }, // 555
    { "Icicle Crash",  TYPE_ICE,      85,  90,  0 }, // 556
    { "V-create",      TYPE_FIRE,     180, 95,  0 }, // 557
    { "Fusion Flare",  TYPE_FIRE,     100, 100, 1 }, // 558
    { "Fusion Bolt",   TYPE_ELECTRIC, 100, 100, 0 }, // 559
};

_Static_assert(sizeof(sGen5Moves) / sizeof(sGen5Moves[0]) == (GEN5_MOVE_LAST - GEN5_MOVE_FIRST + 1),
               "Gen 5 move table size mismatch");

#endif // POKEPLATINUM_PORT_GEN5_MOVES_TABLE_H
