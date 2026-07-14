#ifndef GUARD_CUSTOM_INTRO_H
#define GUARD_CUSTOM_INTRO_H

/* Screen 1: Leaf vs Blue -- Pikachu and Eevee standoff. */
void CB2_StartCustomIntroScreen1(void);

/* Screen 2: Champion Red and Charizard on the mountaintop. */
void CB2_StartCustomIntroScreen2(void);

/* Screen 3: Team Rocket -- Jessie & James, Weezing, Arbok with rose petals. */
void CB2_StartCustomIntroScreen3(void);

/* Screen 4: Legendary birds V-formation, then Mewtwo rises with Mew orbiting. */
void CB2_StartCustomIntroScreen4(void);

/* Screen 5: Dogs jump (Entei/Raikou split BG), then Lugia/Ho-Oh flyby with Suicune. */
void CB2_StartCustomIntroScreen5(void);

/* Screen 6: Deoxys -- galaxy BG, speed flash, form cycle, beam impact. */
void CB2_StartCustomIntroScreen6(void);

/* Shared main loop callback (all screens reuse this). */
void MainCB2_CustomIntro(void);

#endif /* GUARD_CUSTOM_INTRO_H */
