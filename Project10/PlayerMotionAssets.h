#pragma once

// Player movement, attack and idle motion assets.
namespace PlayerMotions {
inline constexpr const char* files[] = {
    "new\\Great Sword Pack\\great sword attack.mv1",
    "new\\Great Sword Pack\\great sword high spin attack.mv1",
    "new\\Great Sword Pack\\great sword jump attack.mv1",
    "new\\Great Sword Pack\\great sword kick.mv1",
    "new\\Great Sword Pack\\great sword kick (2).mv1",
    "new\\Great Sword Pack\\great sword slash.mv1",
    "new\\Great Sword Pack\\great sword slash (2).mv1",
    "new\\Great Sword Pack\\great sword slash (3).mv1",
    "new\\Great Sword Pack\\great sword slash (4).mv1",
    "new\\Great Sword Pack\\great sword slash (5).mv1",
    "new\\Great Sword Pack\\great sword slide attack.mv1",
    "new\\Running.mv1",
    "new\\Great Sword Pack\\great sword idle.mv1",
    "new\\Great Sword Pack\\great sword impact.mv1",
    "new\\Great Sword Pack\\two handed sword death.mv1",
};
inline constexpr int count = sizeof(files) / sizeof(files[0]);
inline constexpr int run = 11;
inline constexpr int idle = 12;
inline constexpr int damage = 13;
inline constexpr int death = 14;
inline constexpr int normalAttack[3] = { 5, 9, 7 };
inline constexpr int heavyAttack = 0;
inline constexpr int specialAttack = 1;
}
