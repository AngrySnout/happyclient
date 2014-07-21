# [happy] Sauerbraten Client V0.9.1 (Alpha)

A custom client for the open-source game [Cube 2: Sauerbraten](http://sauerbraten.org/).

It is designed to be a near perfect client, combining functionality with appearance.

## Features

- Scoreboard:
	1. Show current flag holder.
	2. Show flags scored.
	3. Show frags.
	4. Show deaths.
	5. Show accuracy.
	6. Show KPD.
	7. Show teamkills.
	8. Show spectators' pings.
	9. Show country.
- Server country in server browser.
- Find players.
- Server filter.
- Preview server before connecting.
- Different color for players using auth.
- Auto sorry/np.
- Sorry/np binds (eg
		```
		/bind g [sayteam (concatword "Sorry for the teamkill " $teamkilled "!")]
		```		
		and
		```
		/bind v [sayteam (concatword "No problem " $teamkiller "!")]
		```).
- Can make rifle shots look like lightning.
- Highlight ammo when less than a specific amount.
- Whois player tracking (local database, used like "/whois [cn]" and "/whoisname [name] [exact_ci_match]", or from the master menu).
- Show IP in master menu.
- Chat word highlighting.
- Extra console for frags with a kill icon.
- Quick team chat.
- New background.
- In-game IRC.
- Global stats tracking.
- Quick connect.

## Upcoming

- Dynamic backgrounds from in-game.
- New menus and menu texture.
- Auto record and search through demos.
- Weapon specific kill icons.
- Friends and clans lists.
- Player name auto-complete.
- Event system.
- Cubescript vectors.
- HTTP request support.
- Anti-cheat file verification system with closed source executable.


## Bugs/problems/suggestions:

You can report anything in the [[happy] clan Sauerbraten client](http://happysauerclan.webs.com/apps/forums/topics/show/12939770-happy-clan-sauerbraten-client) thread.

This client is still in alpha. You will run into bugs. Please report any bugs, problems, or suggestions you have on this thread and I will work on fixing/adding them ASAP.
