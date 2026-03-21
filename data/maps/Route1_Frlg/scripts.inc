Route1_Frlg_MapScripts::
	map_script MAP_SCRIPT_ON_TRANSITION, Route1_OnTransition
	.byte 0

Route1_OnTransition::
	setflag FLAG_HIDE_MEW_ON_ROUTE1
	end

Route1_EventScript_MartClerk::
	lock
	faceplayer
	goto_if_set FLAG_GOT_POTION_ON_ROUTE_1, Route1_EventScript_AlreadyGotPotion
	msgbox Route1_Text_WorkAtPokeMartTakeSample
	textcolor NPC_TEXT_COLOR_NEUTRAL
	checkitemspace ITEM_POTION
	goto_if_eq VAR_RESULT, FALSE, EventScript_BagIsFull
	bufferitemname STR_VAR_2, ITEM_POTION
	playfanfare MUS_LEVEL_UP
	message gText_ObtainedTheItem
	waitmessage
	waitfanfare
	additem ITEM_POTION
	msgbox Route1_Text_PutPotionAway
	call EventScript_RestorePrevTextColor
	setflag FLAG_GOT_POTION_ON_ROUTE_1
	release
	end

Route1_EventScript_MewTriggerUp::
	lockall
	setvar VAR_TEMP_1, 0
	clearflag FLAG_HIDE_MEW_ON_ROUTE1
	goto Route1_Mew_Gives_ShinyCharm
	end

Route1_EventScript_MewTriggerMiddle::
	lockall
	setvar VAR_TEMP_1, 1
	clearflag FLAG_HIDE_MEW_ON_ROUTE1
	goto Route1_Mew_Gives_ShinyCharm
	end

Route1_EventScript_MewTriggerDown::
	lockall
	setvar VAR_TEMP_1, 2
	clearflag FLAG_HIDE_MEW_ON_ROUTE1
	goto Route1_Mew_Gives_ShinyCharm
	end

Route1_Mew_Gives_ShinyCharm::
	msgbox Route1_MewWatchingYou, MSGBOX_NPC
	turnobject LOCALID_PLAYER, DIR_EAST
	addobject LOCALID_MEW
	setvar VAR_MEWEVENTROUTE1, 1
	applymovement LOCALID_MEW, Mew_FliesToPlayer
	waitmovement 0
	turnobject LOCALID_PLAYER, DIR_NORTH
	call_if_eq VAR_TEMP_1, 0, Route1_EventScript_MewDown1
	call_if_eq VAR_TEMP_1, 1, Route1_EventScript_MewDown2
	call_if_eq VAR_TEMP_1, 2, Route1_EventScript_MewDown3
	return

Route1_EventScript_MewDown1::
	applymovement LOCALID_MEW, Mew_FliesDown1
	waitmovement 0
	turnobject LOCALID_PLAYER, DIR_WEST
	turnobject LOCALID_MEW, DIR_EAST
	goto Route1_Mew_Gives_ShinyCharm2
	return

Route1_EventScript_MewDown2::
	applymovement LOCALID_MEW, Mew_FliesDown2
	waitmovement 0
	turnobject LOCALID_PLAYER, DIR_WEST
	turnobject LOCALID_MEW, DIR_EAST
	goto Route1_Mew_Gives_ShinyCharm2
	return

Route1_EventScript_MewDown3::
	applymovement LOCALID_MEW, Mew_FliesDown3
	waitmovement 0
	turnobject LOCALID_PLAYER, DIR_WEST
	turnobject LOCALID_MEW, DIR_EAST
	goto Route1_Mew_Gives_ShinyCharm2
	return

Route1_Mew_Gives_ShinyCharm2::
	msgbox Route1_MewWantsToGiveCharm, MSGBOX_NPC
	playfanfare MUS_LEVEL_UP
	bufferitemname STR_VAR_2, ITEM_SHINY_CHARM
	message gText_ObtainedTheItem
	waitmessage
	waitfanfare
	additem ITEM_SHINY_CHARM
	applymovement LOCALID_MEW, Mew_FliesToPlayer
	waitmovement 0
	removeobject LOCALID_MEW
	setflag FLAG_HIDE_MEW_ON_ROUTE1
	releaseall
	end

Route1_EventScript_AlreadyGotPotion::
	msgbox Route1_Text_ComeSeeUsIfYouNeedPokeBalls
	release
	end

Route1_EventScript_Boy::
	msgbox Route1_Text_CanJumpFromLedges, MSGBOX_NPC
	end

Route1_EventScript_RouteSign::
	msgbox Route1_Text_RouteSign, MSGBOX_SIGN
	end

Mew_FliesToPlayer::
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	step_end

Mew_FliesDown1::
	walk_down
	step_end

Mew_FliesDown2::
	walk_down
	walk_down
	step_end

Mew_FliesDown3::
	walk_down
	walk_down
	walk_down
	step_end

Route1_MewWatchingYou::
	.string "You feel like something is\n"
	.string "watching you...$"

Route1_MewWantsToGiveCharm::
	.string "MEW wants to give you\n"
	.string "the SHINY CHARM!$"

Route1_Text_WorkAtPokeMartTakeSample::
	.string "Hi!\n"
	.string "I work at a POKéMON MART.\p"
	.string "It's part of a convenient chain\n"
	.string "selling all sorts of items.\p"
	.string "Please, visit us in VIRIDIAN CITY.\p"
	.string "I know, I'll give you a sample.\n"
	.string "Here you go!$"

Route1_Text_ComeSeeUsIfYouNeedPokeBalls::
	.string "Please come see us if you need\n"
	.string "POKé BALLS for catching POKéMON.$"

Route1_Text_PutPotionAway::
	.string "{PLAYER} put the POTION away in\n"
	.string "the BAG's ITEMS POCKET.$"

Route1_Text_CanJumpFromLedges::
	.string "See those ledges along the road?\p"
	.string "It's a bit scary, but you can jump\n"
	.string "from them.\p"
	.string "You can get back to PALLET TOWN\n"
	.string "quicker that way.$"

Route1_Text_RouteSign::
	.string "ROUTE 1\n"
	.string "PALLET TOWN - VIRIDIAN CITY$"

