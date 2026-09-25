@ Romhack additions that are not shops.

RH_EventScript_MomGivesRings::
	lockall
	textcolor NPC_TEXT_COLOR_FEMALE
	playse SE_PIN
	applymovement LOCALID_PLAYERS_HOUSE_1F_MOM, Common_Movement_ExclamationMark
	waitmovement 0
	applymovement LOCALID_PLAYERS_HOUSE_1F_MOM, Common_Movement_Delay48
	waitmovement 0
	msgbox RH_Text_MomWait
	closemessage
	applymovement LOCALID_PLAYERS_HOUSE_1F_MOM, RH_Movement_MomWalkToPlayer
	waitmovement 0
	applymovement OBJ_EVENT_ID_PLAYER, Common_Movement_FaceDown
	waitmovement 0
	msgbox RH_Text_MomRings
	giveitem ITEM_MEGA_RING
	giveitem ITEM_Z_POWER_RING
	msgbox RH_Text_MomRingsAfter
	closemessage
	applymovement LOCALID_PLAYERS_HOUSE_1F_MOM, RH_Movement_MomWalkBack
	waitmovement 0
	setvar VAR_RH_MOM_GAVE_RINGS, 1
	releaseall
	end

RH_Movement_MomWalkToPlayer:
	walk_up
	walk_right
	walk_right
	face_up
	step_end

RH_Movement_MomWalkBack:
	walk_left
	walk_left
	walk_down
	face_left
	step_end

RH_Text_MomWait::
	.string "MOM: {PLAYER}! Wait a moment!$"

RH_Text_MomRings::
	.string "This package came for you! It's\n"
	.string "from a researcher in a far region.\p"
	.string "They say these rings let POKéMON\n"
	.string "reach powers beyond their limits.$"

RH_Text_MomRingsAfter::
	.string "With the MEGA RING, a POKéMON holding\n"
	.string "a MEGA STONE can MEGA EVOLVE.\p"
	.string "With the Z-POWER RING, one holding a\n"
	.string "Z-CRYSTAL can unleash a Z-MOVE.\p"
	.string "I hear CELADON's DEPT. STORE sells\n"
	.string "those stones and crystals.\p"
	.string "Now go on, PROF. OAK is waiting!$"

@ Safari Zone generation guide (Fuchsia Safari Zone entrance)
RH_EventScript_SafariGenGuide::
	lock
	faceplayer
	special RH_BufferSafariGenName
	msgbox RH_Text_SafariGuideIntro
	message RH_Text_SafariGuideWhich
	waitmessage
	dynmultipush RH_Text_Gen0, 0
	dynmultipush RH_Text_Gen2, 2
	dynmultipush RH_Text_Gen3, 3
	dynmultipush RH_Text_Gen4, 4
	dynmultipush RH_Text_Gen5, 5
	dynmultipush RH_Text_Gen6, 6
	dynmultipush RH_Text_Gen7, 7
	dynmultipush RH_Text_Gen8, 8
	dynmultipush RH_Text_Gen9, 9
	dynmultipush RH_Text_GenAll, 10
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	closemessage
	goto_if_eq VAR_RESULT, MULTI_B_PRESSED, RH_EventScript_SafariGuideCancel
	copyvar VAR_RH_SAFARI_GEN, VAR_RESULT
	special RH_BufferSafariGenName
	msgbox RH_Text_SafariGuideSet
	release
	end

RH_EventScript_SafariGuideCancel::
	msgbox RH_Text_SafariGuideCancel
	release
	end

RH_Text_SafariGuideIntro::
	.string "I'm the SAFARI ZONE's habitat keeper.\p"
	.string "We've brought in POKéMON from far-off\n"
	.string "regions, one generation at a time.\p"
	.string "Water types live in the ponds, and\n"
	.string "each area suits certain types.\p"
	.string "Right now the SAFARI is set to:\n"
	.string "{STR_VAR_1}.$"

RH_Text_SafariGuideWhich::
	.string "Which POKéMON should live here?$"

RH_Text_SafariGuideSet::
	.string "All done! The SAFARI ZONE is now\n"
	.string "set to {STR_VAR_1}.\p"
	.string "CENTER: NORMAL, GRASS, BUG, FAIRY\n"
	.string "EAST: FIRE, ELECTRIC, DRAGON, FLYING\p"
	.string "NORTH: ROCK, GROUND, STEEL,\n"
	.string "FIGHTING and ICE types.\p"
	.string "WEST: POISON, GHOST, PSYCHIC, DARK\n"
	.string "Surf or fish for WATER types!$"

RH_Text_SafariGuideCancel::
	.string "Come back any time!$"

RH_Text_Gen0::
	.string "Classic Kanto$"
RH_Text_Gen2::
	.string "Gen 2 (Johto)$"
RH_Text_Gen3::
	.string "Gen 3 (Hoenn)$"
RH_Text_Gen4::
	.string "Gen 4 (Sinnoh)$"
RH_Text_Gen5::
	.string "Gen 5 (Unova)$"
RH_Text_Gen6::
	.string "Gen 6 (Kalos)$"
RH_Text_Gen7::
	.string "Gen 7 (Alola)$"
RH_Text_Gen8::
	.string "Gen 8 (Galar)$"
RH_Text_Gen9::
	.string "Gen 9 (Paldea)$"
RH_Text_GenAll::
	.string "All gens$"
