#ifndef MARIO_CUSTOM_ANIMS_H_INCLUDED
#define MARIO_CUSTOM_ANIMS_H_INCLUDED

#include <unordered_map>

extern int MARIO_ANIM_CUSTOM_TEST;
extern int MARIO_ANIM_CUSTOM_CLOSE_CAR_DOOR_LEFT;
extern int MARIO_ANIM_CUSTOM_CLOSE_CAR_DOOR_RIGHT;
extern int MARIO_ANIM_CUSTOM_PICKUP_SLOW;
extern int MARIO_ANIM_CUSTOM_PUTDOWN_SLOW;
extern int MARIO_ANIM_CUSTOM_CAR_LOCKED;
extern int MARIO_ANIM_CUSTOM_EAT;
extern int MARIO_ANIM_CUSTOM_VOMIT;
extern int MARIO_ANIM_CUSTOM_CRIB_SWITCH;
extern int MARIO_ANIM_CUSTOM_FACEPALM;
extern int MARIO_ANIM_CUSTOM_LAUGH01;
extern int MARIO_ANIM_CUSTOM_DANCE_LOOP;
extern int MARIO_ANIM_CUSTOM_DANCE_BAD;
extern int MARIO_ANIM_CUSTOM_DANCE_GOOD;
extern int MARIO_ANIM_CUSTOM_VENDING_MACHINE;
extern int MARIO_ANIM_CUSTOM_CLIMB_CJ;
extern int MARIO_ANIM_CUSTOM_STOMP_BELLY;
extern int MARIO_ANIM_CUSTOM_STEALTH_KILL;
extern int MARIO_ANIM_CUSTOM_GUNPOINT;
extern int MARIO_ANIM_CUSTOM_GUNPOINT_WALK_START;
extern int MARIO_ANIM_CUSTOM_GUNPOINT_TIPTOE;
extern int MARIO_ANIM_CUSTOM_GUNPOINT_WALK;
extern int MARIO_ANIM_CUSTOM_GUNPOINT_RUN;
extern int MARIO_ANIM_CUSTOM_GUNPOINT_SKID;
extern int MARIO_ANIM_CUSTOM_GUNPOINT_SKID_STOP;
extern int MARIO_ANIM_CUSTOM_GUNPOINT_SKID_TURN;
extern int MARIO_ANIM_CUSTOM_GUNSIDE_IDLE;
extern int MARIO_ANIM_CUSTOM_GUNSIDE_IDLE_ALT;
extern int MARIO_ANIM_CUSTOM_GUNSIDE_WALK_START;
extern int MARIO_ANIM_CUSTOM_GUNSIDE_TIPTOE;
extern int MARIO_ANIM_CUSTOM_GUNSIDE_WALK;
extern int MARIO_ANIM_CUSTOM_GUNSIDE_RUN;
extern int MARIO_ANIM_CUSTOM_GUNSIDE_SKID;
extern int MARIO_ANIM_CUSTOM_GUNSIDE_SKID_STOP;
extern int MARIO_ANIM_CUSTOM_GUNSIDE_SKID_TURN;
extern int MARIO_ANIM_CUSTOM_RPG_IDLE;
extern int MARIO_ANIM_CUSTOM_RPG_IDLE_ALT;
extern int MARIO_ANIM_CUSTOM_RPG_WALK_START;
extern int MARIO_ANIM_CUSTOM_RPG_TIPTOE;
extern int MARIO_ANIM_CUSTOM_RPG_WALK;
extern int MARIO_ANIM_CUSTOM_RPG_RUN;
extern int MARIO_ANIM_CUSTOM_RPG_SKID;
extern int MARIO_ANIM_CUSTOM_RPG_SKID_STOP;
extern int MARIO_ANIM_CUSTOM_RPG_SKID_TURN;
extern int MARIO_ANIM_CUSTOM_GUNHEAVY_IDLE;
extern int MARIO_ANIM_CUSTOM_GUNHEAVY_IDLE_ALT;
extern int MARIO_ANIM_CUSTOM_GUNHEAVY_WALK_START;
extern int MARIO_ANIM_CUSTOM_GUNHEAVY_TIPTOE;
extern int MARIO_ANIM_CUSTOM_GUNHEAVY_WALK;
extern int MARIO_ANIM_CUSTOM_GUNHEAVY_RUN;
extern int MARIO_ANIM_CUSTOM_GUNHEAVY_SKID;
extern int MARIO_ANIM_CUSTOM_GUNHEAVY_SKID_STOP;
extern int MARIO_ANIM_CUSTOM_GUNHEAVY_SKID_TURN;
extern int MARIO_ANIM_CUSTOM_RIFLE_AIM;
extern int MARIO_ANIM_CUSTOM_RIFLE_AIM_WALK;
extern int MARIO_ANIM_CUSTOM_GUNHEAVY_AIM;
extern int MARIO_ANIM_CUSTOM_GUNHEAVY_AIM_WALK;
extern int MARIO_ANIM_CUSTOM_GUNLIGHT_AIM;
extern int MARIO_ANIM_CUSTOM_GUNLIGHT_AIM_WALK;

extern std::unordered_map<int, int> gunAnimOverrideTable;
extern std::unordered_map<int, int> gunSideAnimOverrideTable;
extern std::unordered_map<int, int> gunShoulderAnimOverrideTable;
extern std::unordered_map<int, int> gunHeavyAnimOverrideTable;

void marioInitCustomAnims();

#endif // MARIO_CUSTOM_ANIMS_H_INCLUDED
