@ Romhack additions that are not shops.

RH_EventScript_MomGivesRings::
	lockall
	textcolor NPC_TEXT_COLOR_FEMALE
	msgbox RH_Text_MomWait
	closemessage
	msgbox RH_Text_MomRings
	giveitem ITEM_MEGA_RING
	giveitem ITEM_Z_POWER_RING
	msgbox RH_Text_MomRingsAfter
	closemessage
	setvar VAR_RH_MOM_GAVE_RINGS, 1
	releaseall
	end

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
