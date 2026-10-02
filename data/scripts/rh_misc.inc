@ Romhack additions that are not shops.

@ Randomizer Settings key item (GBA SP): warning, then the settings screen (special waits until it closes).
RH_EventScript_RandomizerSettings::
	lockall
	textcolor NPC_TEXT_COLOR_NEUTRAL
	msgbox RH_Text_RandomizerSettingsWarning, MSGBOX_DEFAULT
	msgbox RH_Text_RandomizerSettingsOpen, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, RH_EventScript_RandomizerSettingsEnd
	closemessage
	fadescreen FADE_TO_BLACK
	special RH_OpenRandomizerSettings
	waitstate
RH_EventScript_RandomizerSettingsEnd:
	releaseall
	end

RH_Text_RandomizerSettingsWarning::
	.string "WARNING!\n"
	.string "Changing the settings mid-run\p"
	.string "can BREAK your current playthrough.\n"
	.string "POKéMON, trainers and items may\l"
	.string "all change.\p"
	.string "Be careful when using this!$"

RH_Text_RandomizerSettingsOpen::
	.string "Open the RANDOMIZER SETTINGS?$"

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
	msgbox RH_Text_MomRunningShoes
	call RH_EventScript_GetRunningShoes
	closemessage
	applymovement LOCALID_PLAYERS_HOUSE_1F_MOM, RH_Movement_MomWalkBack
	waitmovement 0
	setvar VAR_RH_MOM_GAVE_RINGS, 1
	releaseall
	end

RH_EventScript_GetRunningShoes::
	textcolor NPC_TEXT_COLOR_NEUTRAL
	playfanfare MUS_RG_OBTAIN_KEY_ITEM
	message RH_Text_ReceivedRunningShoesFromMom
	waitmessage
	waitfanfare
	signmsg
	msgbox PewterCity_Text_ExplainRunningShoes
	normalmsg
	setflag FLAG_SYS_B_DASH
	textcolor NPC_TEXT_COLOR_FEMALE
	return

@ ---------------------------------------------------------------------------
@ The level 100 Mewtwo blocking Pewter City's east road until BROCK is beaten.
@ ---------------------------------------------------------------------------
RH_EventScript_PewterMewtwo::
	lockall
	goto RH_EventScript_PewterMewtwoMeet
	end

RH_EventScript_PewterMewtwoTrigger::
	lockall
	applymovement OBJ_EVENT_ID_PLAYER, Common_Movement_FaceRight
	waitmovement 0
	goto RH_EventScript_PewterMewtwoMeet
	end

RH_EventScript_PewterMewtwoMeet::
	playmoncry SPECIES_MEWTWO, CRY_MODE_ENCOUNTER
	msgbox RH_Text_GoFightBrockDude
	waitmoncry
	msgbox RH_Text_FightMewtwoAnyway, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, RH_EventScript_PewterMewtwoStepBack
	closemessage
	setflag FLAG_RH_PEWTER_MEWTWO_BATTLE
	setwildbattle SPECIES_MEWTWO, 100
	special RH_PreparePewterMewtwo
	dowildbattle
	clearflag FLAG_RH_PEWTER_MEWTWO_BATTLE
	specialvar VAR_RESULT, GetBattleOutcome
	goto_if_eq VAR_RESULT, B_OUTCOME_WON, RH_EventScript_PewterMewtwoGone
	goto_if_eq VAR_RESULT, B_OUTCOME_CAUGHT, RH_EventScript_PewterMewtwoGone
	goto RH_EventScript_PewterMewtwoStepBack
	end

RH_EventScript_PewterMewtwoStepBack::
	closemessage
	applymovement OBJ_EVENT_ID_PLAYER, RH_Movement_StepBackWest
	waitmovement 0
	releaseall
	end

RH_EventScript_PewterMewtwoGone::
	removeobject LOCALID_PEWTER_GYM_GUIDE
	setflag FLAG_HIDE_PEWTER_CITY_GYM_GUIDE
	clearflag FLAG_RH_HIDE_ROUTE3_BARRICADE
	setvar VAR_MAP_SCENE_PEWTER_CITY, 3
	releaseall
	end

RH_Movement_StepBackWest:
	face_left
	walk_left
	step_end

RH_EventScript_Route3Barricade::
	msgbox RH_Text_Route3Barricade, MSGBOX_SIGN
	end

RH_Text_GoFightBrockDude::
	.string "MEWTWO: Go fight Brock, dude...$"

RH_Text_FightMewtwoAnyway::
	.string "Fight MEWTWO anyway?$"

RH_Text_Route3Barricade::
	.string "The road is barricaded.\p"
	.string "ROAD CLOSED until the PEWTER GYM\n"
	.string "LEADER has been defeated.$"

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

RH_Text_MomRunningShoes::
	.string "Oh, and PROF. OAK's AIDE dropped\n"
	.string "these off for you, too.\p"
	.string "RUNNING SHOES! Now you can dash\n"
	.string "around everywhere, even indoors!$"

RH_Text_ReceivedRunningShoesFromMom::
	.string "{PLAYER} received the\n"
	.string "RUNNING SHOES from MOM.$"

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
